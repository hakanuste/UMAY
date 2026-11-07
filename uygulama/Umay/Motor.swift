import Foundation
#if os(macOS)
import AppKit        // YALNIZ macOS: iOS'ta AppKit YOKTUR.
#endif
import UserNotifications

/// UMAY'ın C çekirdeğini saran sınıf. Çekirdek tek iplikli olduğundan bütün çağrılar tek sıradan geçer.
final class UmayMotor: ObservableObject {
    struct Ileti: Identifiable, Codable {
        var id = UUID()
        let benMi: Bool
        let metin: String
        var sistemMi = false
    }

    @Published var iletiler: [Ileti] = [] { didSet { gecmisiYaz() } }
    @Published var mesgul = false
    @Published var sonSure = ""
    /// Son "doğrulanmadı" notlu yanıtın çifti; kullanıcı "doğru, ekle" deyip "Eklendi" gelirse eğitim dosyasına yazılır.
    private var bekleyenCift: (soru: String, yanit: String)?
    @Published var durum = "Başlatılıyor…"
    @Published var motorAdi = "…"
    @Published var modelDosyasi = ""

    let sira = DispatchQueue(label: "umay.motor")
    private var basladi = false

    /// Veri dizini: uygulamanın yazabildiği yer (BENGİ belleği ve kütüphane buradadır).
    static var kutDizini: URL {
        let taban = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
        return taban.appendingPathComponent("Umay/kut", isDirectory: true)
    }
    static var modelDizini: URL { kutDizini.appendingPathComponent("model", isDirectory: true) }
    static var gelenDizini: URL { kutDizini.appendingPathComponent("gelen", isDirectory: true) }

    init() { gecmisiOku(); baslat() }

    // MARK: sohbet geçmişi (yalnız bu aygıtta, kullanıcının veri dizininde)

    private static var gecmisDosyasi: URL { kutDizini.deletingLastPathComponent().appendingPathComponent("sohbet.json") }

    private func gecmisiOku() {
        guard let d = try? Data(contentsOf: UmayMotor.gecmisDosyasi),
              let l = try? JSONDecoder().decode([Ileti].self, from: d) else { return }
        iletiler = l
    }

    private func gecmisiYaz() {
        let son = Array(iletiler.suffix(200))     // en çok son 200 ileti
        try? FileManager.default.createDirectory(at: UmayMotor.kutDizini.deletingLastPathComponent(), withIntermediateDirectories: true)
        if let d = try? JSONEncoder().encode(son) { try? d.write(to: UmayMotor.gecmisDosyasi, options: .atomic) }
    }

    func temizle() { iletiler = [] }

    /// Yanıtın künye satırlarını ("X söyler:", "(Dayanağım…)", "Güven:") gizler; yalnız cevap kalır.
    /// Ayarlar'da "Kaynak ve güven satırlarını göster" açıksa olduğu gibi bırakır. Yanıtın ham hâli çekirdekte durur.
    static func sade(_ yanit: String) -> String {
        if UserDefaults.standard.bool(forKey: "umay.kunye") { return yanit }
        var yanit = yanit
        if let r = yanit.range(of: "CEVAP:", options: .backwards) {       // modelin yankıladığı istem iskeleti
            let son = yanit[r.upperBound...].trimmingCharacters(in: .whitespacesAndNewlines)
            if !son.isEmpty { yanit = son }
        }
        let satirlar = yanit.split(separator: "\n", omittingEmptySubsequences: false).map(String.init)
        let kalan = satirlar.filter { !Okuyucu.kunyeSatiri($0) }
        let metin = kalan.joined(separator: "\n").trimmingCharacters(in: .whitespacesAndNewlines)
        return metin.isEmpty ? yanit : metin
    }

    // MARK: başlatma

    /// "İnternetten kendiliğinden öğren" ayarını ortama yazar (çekirdek her çağrıda okur).
    private static var varsayilanKayitli = false
    static func agAyariniUygula() {
        // register(defaults:) bir bildirim yollar; bu işlev o bildirimi dinleyen gözlemciden de çağrıldığı için
        // her seferinde yeniden kaydetmek sonsuz özyinelemeye (yığın taşması) yol açıyordu. Yalnız bir kez kaydet.
        if !varsayilanKayitli {
            varsayilanKayitli = true
            UserDefaults.standard.register(defaults: ["umay.ogren": true])   // varsayılan: kayıt yoksa internetten ara
        }
        if UserDefaults.standard.bool(forKey: "umay.ogren") { unsetenv("UMAY_OGREN") }
        else { setenv("UMAY_OGREN", "0", 1) }
    }

    func baslat() {
        sira.async { [weak self] in
            guard let self = self else { return }
            let fm = FileManager.default
            let kut = UmayMotor.kutDizini
            try? fm.createDirectory(at: UmayMotor.modelDizini, withIntermediateDirectories: true)
            try? fm.createDirectory(at: UmayMotor.gelenDizini, withIntermediateDirectories: true)
            // Uygulama paketindeki kut/ içeriğini ilk açılışta kopyala; kullanıcının dosyalarının üstüne yazma.
            if let paket = Bundle.main.resourceURL?.appendingPathComponent("kut"),
               let adlar = try? fm.contentsOfDirectory(atPath: paket.path) {
                for ad in adlar {
                    let hedef = kut.appendingPathComponent(ad)
                    if !fm.fileExists(atPath: hedef.path) {
                        try? fm.copyItem(at: paket.appendingPathComponent(ad), to: hedef)
                    }
                }
            }
            // Paketteki kaynak belgeleri (docs/kaynak) gelen/ klasörüne bırak; KABUL ajanı kendiliğinden
            // içeri alır (işlenmiş belge ikinci kez alınmaz). Elle kopyalama gerekmez.
            if let kaynak = Bundle.main.resourceURL?.appendingPathComponent("kaynak"),
               let adlar = try? fm.contentsOfDirectory(atPath: kaynak.path) {
                for ad in adlar where ad.hasSuffix(".txt") || ad.hasSuffix(".md") {
                    let hedef = UmayMotor.gelenDizini.appendingPathComponent(ad)
                    if !fm.fileExists(atPath: hedef.path) {
                        try? fm.copyItem(at: kaynak.appendingPathComponent(ad), to: hedef)
                    }
                }
            }
            // Model: kut/model içindeki ilk .gguf; seçilmişse o.
            let secili = UserDefaults.standard.string(forKey: "umay.model") ?? ""
            let dosyalar = (try? fm.contentsOfDirectory(atPath: UmayMotor.modelDizini.path)) ?? []
            let ggufler = dosyalar.filter { $0.hasSuffix(".gguf") }.sorted()
            let model = ggufler.contains(secili) ? secili : (ggufler.first ?? "")
            if model.isEmpty { unsetenv("UMAY_LLM") }
            else { setenv("UMAY_LLM", UmayMotor.modelDizini.appendingPathComponent(model).path, 1) }
            // İnternetten öğrenme: varsayılan KAPALI (telefonda sessizce ağ kullanılmasın).
            UmayMotor.agAyariniUygula()
            // Ayarlardan değişince yeniden başlatmaya gerek kalmadan hemen işler.
            NotificationCenter.default.addObserver(forName: UserDefaults.didChangeNotification,
                                                   object: nil, queue: nil) { _ in
                UmayMotor.agAyariniUygula()
            }

            umay_gomulu_ag_ata(agGetirici)
            let kod = umay_gomulu_baslat(kut.path)
            let ad = kod == 0 ? String(cString: umay_gomulu_motor()) : "yok"
            DispatchQueue.main.async {
                self.basladi = kod == 0
                self.motorAdi = ad
                self.modelDosyasi = model
                self.durum = kod == 0 ? "" : "UMAY başlatılamadı."
                self.gelenYokla()
                if kod == 0 { self.animiBaslat() }
            }
        }
    }

    // MARK: anımsatıcı ve ilaç alarmı (çekirdek ANIM; kayıt yalnız bu aygıtta)

    private var animZamanlayici: Timer?

    /// 20 saniyede bir çekirdeğe "vakti gelen var mı" diye sorar; varsa sohbete yazar, bildirim gösterir, ses çalar.
    func animiBaslat() {
        guard animZamanlayici == nil else { return }
        UNUserNotificationCenter.current().requestAuthorization(options: [.alert, .sound]) { _, _ in }
        animZamanlayici = Timer.scheduledTimer(withTimeInterval: 20, repeats: true) { [weak self] _ in
            guard let self = self, self.basladi else { return }
            self.sira.async {
                var tampon = [CChar](repeating: 0, count: 16384)
                let k = umay_gomulu_anim_yokla(&tampon, UInt32(tampon.count))
                guard k > 0 else { return }
                let satirlar = String(cString: tampon).split(separator: "\u{1e}").map(String.init)
                DispatchQueue.main.async {
                    for satir in satirlar {
                        self.iletiler.append(Ileti(benMi: false, metin: satir))
                        let icerik = UNMutableNotificationContent()
                        // Çok satırlı metin sabah bildirisidir: bildirimde yalnız kısa haber, tamamı sohbette.
                        let bildiri = satir.contains("\n")
                        icerik.title = bildiri ? "UMAY — Sabah bildirisi" : "UMAY"
                        icerik.body = bildiri ? "Hazır; sohbete yazdım." : String(satir.prefix(180))
                        icerik.sound = .default
                        UNUserNotificationCenter.current().add(UNNotificationRequest(identifier: UUID().uuidString, content: icerik, trigger: nil))
                        // SESLİ: uyarı sesi, sonra Dede Korkut sesiyle okunur (Ayarlar'dan kapatılabilir).
                        // NSSound AppKit'e özgüdür, iOS'ta yoktur. iOS'ta
                        // bildirinin kendi sesi çalar (yukarıda
                        // icerik.sound = .default), ek sese gerek kalmaz.
                        #if os(macOS)
                        NSSound(named: NSSound.Name("Glass"))?.play()
                        #endif
                        if (UserDefaults.standard.object(forKey: "umay.alarmSesli") as? Bool) ?? true {
                            Okuyucu.ortak.oku(bildiri ? "Sabah bildirisi hazır." : satir)
                        }
                    }
                }
            }
        }
    }

    // MARK: soru

    func sor(_ soru: String) {
        let s = soru.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !s.isEmpty, !mesgul, basladi else { return }
        iletiler.append(Ileti(benMi: true, metin: s))
        mesgul = true
        let t0 = Date()
        sira.async { [weak self] in
            var tampon = [CChar](repeating: 0, count: 16384)
            let n = umay_gomulu_sor(s, &tampon, UInt32(tampon.count))
            let yanit = n > 0 ? String(cString: tampon) : "Yanıt alınamadı."
            let ad = String(cString: umay_gomulu_motor())
            DispatchQueue.main.async {
                guard let self = self else { return }
                self.onayliCiftiIzle(soru: s, yanit: yanit)
                self.iletiler.append(Ileti(benMi: false, metin: UmayMotor.sade(yanit)))
                self.motorAdi = ad
                self.mesgul = false
                self.sonSure = String(format: "yanıt %.1f sn", Date().timeIntervalSince(t0))
                Okuyucu.ortak.istekAni = Date()
                Okuyucu.ortak.sesBasladi = { [weak self] d, tur in
                    self?.sonSure += String(format: " · ses +%.1f sn (%@)", d, tur)
                }
                // Hem yazılı hem sesli: ayar kapatılmadıkça yanıt okunur.
                if (UserDefaults.standard.object(forKey: "umay.sesli") as? Bool) ?? true, n > 0 {
                    Okuyucu.ortak.kendiliginden(yanit)
                }
            }
        }
    }

    // MARK: onaylı soru–yanıt çiftleri (yerel eğitim verisi)

    private static var onayliDosyasi: URL {
        kutDizini.deletingLastPathComponent().appendingPathComponent("egitim/onayli.jsonl")
    }

    /// Yalnız kullanıcının AÇIKÇA onayladığı çiftler: önce "(doğrulanmadı)" notlu bir yanıt gelir, kullanıcı "doğru…" der,
    /// çekirdek "Eklendi" diye yanıtlarsa o çift yerel dosyaya eklenir. Dosya makineden çıkmaz; dışa aktarma ayrı ve elle yapılır.
    private func onayliCiftiIzle(soru: String, yanit: String) {
        let kucuk = soru.lowercased()
        let onay = kucuk.hasPrefix("doğru") || kucuk.hasPrefix("dogru")
        let ret = kucuk.hasPrefix("yanlış") || kucuk.hasPrefix("yanlis")
        if !onay && !ret { lastSoru = soru }      // önce güncelle: bu yanıt bu soruya aittir
        if yanit.contains("doğrulanmadı") {
            bekleyenCift = (soru: lastSoru ?? "", yanit: yanit)
        } else if let c = bekleyenCift, !c.soru.isEmpty, onay, yanit.contains("Eklendi") {
            let satir: [String: String] = ["soru": c.soru, "yanit": UmayMotor.sade(c.yanit)]
            if let d = try? JSONSerialization.data(withJSONObject: satir, options: [.withoutEscapingSlashes]),
               var metin = String(data: d, encoding: .utf8) {
                metin += "\n"
                let yol = UmayMotor.onayliDosyasi
                try? FileManager.default.createDirectory(at: yol.deletingLastPathComponent(), withIntermediateDirectories: true)
                if let h = try? FileHandle(forWritingTo: yol) { h.seekToEndOfFile(); h.write(Data(metin.utf8)); try? h.close() }
                else { try? metin.write(to: yol, atomically: true, encoding: .utf8) }
            }
            bekleyenCift = nil
        } else if ret {
            bekleyenCift = nil
        }
    }
    private var lastSoru: String?

    // MARK: belge ekleme (KABUL)

    /// gelen/ klasöründe işlenmemiş belge varsa ajanlara inceletir (açılışta ve uygulama öne gelince).
    func gelenYokla() {
        guard basladi, !mesgul else { return }
        gelenAdlariniKisalt()
        let adlar = (try? FileManager.default.contentsOfDirectory(atPath: UmayMotor.gelenDizini.path)) ?? []
        guard adlar.contains(where: { !$0.hasPrefix("_") && !$0.hasPrefix(".") }) else { return }
        mesgul = true
        sira.async { [weak self] in
            UmayMotor.taranmisPdfleriOku { m in DispatchQueue.main.async { self?.iletiler.append(Ileti(benMi: false, metin: m, sistemMi: true)) } }
            var rapor = [CChar](repeating: 0, count: 8192)
            let n = umay_gomulu_kabul(&rapor, UInt32(rapor.count))
            let metin = String(cString: rapor)
            DispatchQueue.main.async {
                if n > 0 { self?.iletiler.append(Ileti(benMi: false, metin: metin, sistemMi: true)) }
                self?.mesgul = false
            }
        }
    }

    /// Çekirdek dizin girdisi adını 32 bayta sığdırır (ÖTÜKEN ABI'si); daha uzun adlar görünmez olur.
    /// Ad: sadeleştirilmiş baş + 8 haneli özet + uzantı. Aynı dosya hep aynı kısa adı alır.
    static func kisaAd(_ ad: String) -> String {
        if ad.utf8.count < 32 { return ad }
        let u = (ad as NSString).pathExtension.lowercased()
        let govde = (ad as NSString).deletingPathExtension
        var h: UInt32 = 2166136261
        for b in govde.utf8 { h = (h ^ UInt32(b)) &* 16777619 }
        let harita: [Character: Character] = ["ç":"c","ğ":"g","ı":"i","ö":"o","ş":"s","ü":"u","â":"a","î":"i","û":"u"]
        var kok = ""
        for c in govde.lowercased() {
            if kok.count >= 14 { break }
            if let k = harita[c] { kok.append(k) }
            else if c.isASCII && (c.isLetter || c.isNumber) { kok.append(c) }
            else if c == " " || c == "-" || c == "_" { if !kok.hasSuffix("-") { kok.append("-") } }
        }
        let son = String(format: "%08x", h)
        return u.isEmpty ? "\(kok)-\(son)" : "\(kok)-\(son).\(u)"
    }

    /// Kısa ad -> özgün ad eşlemesini gelen/_adlar.txt'ye yazar (KABUL kaynakta kitabın adını gösterir).
    static func adiKaydet(kisa: String, ozgun: String, alan: String = "") {
        guard kisa != ozgun || !alan.isEmpty else { return }
        let yol = gelenDizini.appendingPathComponent("_adlar.txt")
        let ek = alan.isEmpty ? "" : "\t\(alan)"
        let satir = "\(kisa)\t\(ozgun.replacingOccurrences(of: "\n", with: " "))\(ek)\n"
        if let d = satir.data(using: .utf8) {
            if let h = try? FileHandle(forWritingTo: yol) { h.seekToEndOfFile(); h.write(d); try? h.close() }
            else { try? d.write(to: yol) }
        }
    }

    /// gelen/ içindeki fazla uzun adlı dosyaları kısa adla yeniden adlandırır (aksi halde hiç görülmezler).
    func gelenAdlariniKisalt() {
        let fm = FileManager.default
        let adlar = (try? fm.contentsOfDirectory(atPath: UmayMotor.gelenDizini.path)) ?? []
        for ad in adlar where !ad.hasPrefix(".") && ad.utf8.count >= 32 {
            let kaynak = UmayMotor.gelenDizini.appendingPathComponent(ad)
            let hedef = UmayMotor.gelenDizini.appendingPathComponent(UmayMotor.kisaAd(ad))
            try? fm.removeItem(at: hedef)
            if (try? fm.moveItem(at: kaynak, to: hedef)) != nil {
                UmayMotor.adiKaydet(kisa: hedef.lastPathComponent, ozgun: ad)
            }
        }
    }

    func belgeEkle(_ adresler: [URL], alan: String = "") {
        let fm = FileManager.default
        var notlar: [String] = []
        for a in adresler {
            let erisim = a.startAccessingSecurityScopedResource()
            defer { if erisim { a.stopAccessingSecurityScopedResource() } }
            let hedef = UmayMotor.gelenDizini.appendingPathComponent(UmayMotor.kisaAd(a.lastPathComponent))
            let boyut = ((try? fm.attributesOfItem(atPath: a.path)[.size]) as? NSNumber)?.intValue ?? -1
            try? fm.removeItem(at: hedef)
            do {
                try fm.copyItem(at: a, to: hedef)
                UmayMotor.adiKaydet(kisa: hedef.lastPathComponent, ozgun: a.lastPathComponent, alan: alan)
                notlar.append("✓ \(a.lastPathComponent) alındı (\(boyut / 1024) KB)")
            } catch {
                // Sessizce yutma: neden eklenemediğini kullanıcıya söyle.
                notlar.append("✗ \(a.lastPathComponent) kopyalanamadı: \(error.localizedDescription)")
            }
        }
        mesgul = true
        sira.async { [weak self] in
            UmayMotor.taranmisPdfleriOku { m in DispatchQueue.main.async { self?.iletiler.append(Ileti(benMi: false, metin: m, sistemMi: true)) } }
            var rapor = [CChar](repeating: 0, count: 8192)
            _ = umay_gomulu_kabul(&rapor, UInt32(rapor.count))
            let metin = String(cString: rapor)
            let tam = (notlar + [metin.isEmpty ? "KABUL: rapor boş döndü (belge işlenemedi ya da dosya bulunamadı)." : metin]).joined(separator: "\n")
            DispatchQueue.main.async {
                self?.iletiler.append(Ileti(benMi: false, metin: tam, sistemMi: true))
                self?.mesgul = false
            }
        }
    }

    // MARK: sürükle-bırak ve pano

    /// Pencereye bırakılan dosyaları (txt, md, pdf) ve düz metni belge olarak alır. Sohbet dökümünü seçip bırakmak yeter.
    func birakilanlariAl(_ saglayicilar: [NSItemProvider], alan: String) {
        let kilit = NSLock()
        var adresler: [URL] = []
        var metinler: [String] = []
        let grup = DispatchGroup()
        for s in saglayicilar {
            if s.hasItemConformingToTypeIdentifier("public.file-url") {
                grup.enter()
                s.loadItem(forTypeIdentifier: "public.file-url", options: nil) { oge, _ in
                    var adres: URL? = nil
                    if let d = oge as? Data { adres = URL(dataRepresentation: d, relativeTo: nil) }
                    else if let u = oge as? URL { adres = u }
                    if let a = adres { kilit.lock(); adresler.append(a); kilit.unlock() }
                    grup.leave()
                }
            } else if s.canLoadObject(ofClass: NSString.self) {
                grup.enter()
                _ = s.loadObject(ofClass: NSString.self) { nesne, _ in
                    if let m = nesne as? NSString { kilit.lock(); metinler.append(m as String); kilit.unlock() }
                    grup.leave()
                }
            }
        }
        grup.notify(queue: .main) { [weak self] in
            guard let self = self else { return }
            let desteklenen = Set(["txt", "md", "pdf"])
            let uygun = adresler.filter { desteklenen.contains($0.pathExtension.lowercased()) }
            let atilan = adresler.filter { !desteklenen.contains($0.pathExtension.lowercased()) }
            if !atilan.isEmpty {
                let adlar = atilan.map { $0.lastPathComponent }.joined(separator: ", ")
                self.iletiler.append(Ileti(benMi: false, metin: "Şunları okuyamam (yalnız .txt, .md, .pdf): \(adlar)", sistemMi: true))
            }
            if !uygun.isEmpty { self.belgeEkle(uygun, alan: alan) }
            for m in metinler where m.trimmingCharacters(in: .whitespacesAndNewlines).count > 40 { self.metinBelgesiEkle(m, alan: alan) }
        }
    }

    /// Metni (örneğin panodaki bir sohbet dökümünü) belge olarak ekler.
    func metinBelgesiEkle(_ metin: String, alan: String = "") {
        let bicim = DateFormatter(); bicim.dateFormat = "yyyyMMdd-HHmmss"
        let ad = "yapistirilan-\(bicim.string(from: Date())).txt"
        let gecici = FileManager.default.temporaryDirectory.appendingPathComponent(ad)
        do {
            try metin.write(to: gecici, atomically: true, encoding: .utf8)
            belgeEkle([gecici], alan: alan)
        } catch {
            iletiler.append(Ileti(benMi: false, metin: "Metin belge olarak yazılamadı: \(error.localizedDescription)", sistemMi: true))
        }
    }

    var gercekModel: Bool { motorAdi == "llama" }
}

/// Ağ getirici: UMAY web araştırması isterse çağrılır (varsayılan kapalıdır).
private let agGetirici: @convention(c) (UnsafePointer<CChar>?, UnsafeMutablePointer<CChar>?, UInt32) -> Int64 = { url, cevap, n in
    guard let url = url, let cevap = cevap, n > 1, let adres = URL(string: String(cString: url)) else { return -1 }
    var istek = URLRequest(url: adres, timeoutInterval: 15)
    istek.setValue("UMAY/1.0", forHTTPHeaderField: "User-Agent")
    let bekle = DispatchSemaphore(value: 0)
    var veri: Data?
    URLSession.shared.dataTask(with: istek) { d, r, _ in
        if let h = r as? HTTPURLResponse, h.statusCode == 200 { veri = d }
        bekle.signal()
    }.resume()
    bekle.wait()
    guard let v = veri else { return -1 }
    let k = min(v.count, Int(n) - 1)
    v.copyBytes(to: UnsafeMutableRawBufferPointer(start: UnsafeMutableRawPointer(cevap), count: k))
    cevap[k] = 0
    return Int64(k)
}

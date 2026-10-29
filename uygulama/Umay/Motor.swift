import Foundation

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
    @Published var durum = "Başlatılıyor…"
    @Published var motorAdi = "…"
    @Published var modelDosyasi = ""

    private let sira = DispatchQueue(label: "umay.motor")
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

    // MARK: başlatma

    /// "İnternetten kendiliğinden öğren" ayarını ortama yazar (çekirdek her çağrıda okur).
    static func agAyariniUygula() {
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
            }
        }
    }

    // MARK: soru

    func sor(_ soru: String) {
        let s = soru.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !s.isEmpty, !mesgul, basladi else { return }
        iletiler.append(Ileti(benMi: true, metin: s))
        mesgul = true
        sira.async { [weak self] in
            var tampon = [CChar](repeating: 0, count: 16384)
            let n = umay_gomulu_sor(s, &tampon, UInt32(tampon.count))
            let yanit = n > 0 ? String(cString: tampon) : "Yanıt alınamadı."
            let ad = String(cString: umay_gomulu_motor())
            DispatchQueue.main.async {
                guard let self = self else { return }
                self.iletiler.append(Ileti(benMi: false, metin: yanit))
                self.motorAdi = ad
                self.mesgul = false
                // Hem yazılı hem sesli: ayar kapatılmadıkça yanıt okunur.
                if (UserDefaults.standard.object(forKey: "umay.sesli") as? Bool) ?? true, n > 0 {
                    Okuyucu.ortak.kendiliginden(yanit)
                }
            }
        }
    }

    // MARK: belge ekleme (KABUL)

    /// gelen/ klasöründe işlenmemiş belge varsa ajanlara inceletir (açılışta ve uygulama öne gelince).
    func gelenYokla() {
        guard basladi, !mesgul else { return }
        gelenAdlariniKisalt()
        let adlar = (try? FileManager.default.contentsOfDirectory(atPath: UmayMotor.gelenDizini.path)) ?? []
        guard adlar.contains(where: { !$0.hasPrefix("_") && !$0.hasPrefix(".") }) else { return }
        mesgul = true
        sira.async { [weak self] in
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

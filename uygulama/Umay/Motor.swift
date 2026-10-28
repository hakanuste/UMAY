import Foundation

/// UMAY'ın C çekirdeğini saran sınıf. Çekirdek tek iplikli olduğundan bütün çağrılar tek sıradan geçer.
final class UmayMotor: ObservableObject {
    struct Ileti: Identifiable {
        let id = UUID()
        let benMi: Bool
        let metin: String
        var sistemMi = false
    }

    @Published var iletiler: [Ileti] = []
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

    init() { baslat() }

    // MARK: başlatma

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
            // Model: kut/model içindeki ilk .gguf; seçilmişse o.
            let secili = UserDefaults.standard.string(forKey: "umay.model") ?? ""
            let dosyalar = (try? fm.contentsOfDirectory(atPath: UmayMotor.modelDizini.path)) ?? []
            let ggufler = dosyalar.filter { $0.hasSuffix(".gguf") }.sorted()
            let model = ggufler.contains(secili) ? secili : (ggufler.first ?? "")
            if model.isEmpty { unsetenv("UMAY_LLM") }
            else { setenv("UMAY_LLM", UmayMotor.modelDizini.appendingPathComponent(model).path, 1) }
            // İnternetten öğrenme: varsayılan KAPALI (telefonda sessizce ağ kullanılmasın).
            if UserDefaults.standard.bool(forKey: "umay.ogren") { unsetenv("UMAY_OGREN") }
            else { setenv("UMAY_OGREN", "0", 1) }

            umay_gomulu_ag_ata(agGetirici)
            let kod = umay_gomulu_baslat(kut.path)
            let ad = kod == 0 ? String(cString: umay_gomulu_motor()) : "yok"
            DispatchQueue.main.async {
                self.basladi = kod == 0
                self.motorAdi = ad
                self.modelDosyasi = model
                self.durum = kod == 0 ? "" : "UMAY başlatılamadı."
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
            }
        }
    }

    // MARK: belge ekleme (KABUL)

    func belgeEkle(_ adresler: [URL]) {
        let fm = FileManager.default
        for a in adresler {
            let erisim = a.startAccessingSecurityScopedResource()
            defer { if erisim { a.stopAccessingSecurityScopedResource() } }
            let hedef = UmayMotor.gelenDizini.appendingPathComponent(a.lastPathComponent)
            try? fm.removeItem(at: hedef)
            try? fm.copyItem(at: a, to: hedef)
        }
        mesgul = true
        sira.async { [weak self] in
            var rapor = [CChar](repeating: 0, count: 8192)
            _ = umay_gomulu_kabul(&rapor, UInt32(rapor.count))
            let metin = String(cString: rapor)
            DispatchQueue.main.async {
                self?.iletiler.append(Ileti(benMi: false, metin: metin.isEmpty ? "Belge işlenemedi." : metin, sistemMi: true))
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

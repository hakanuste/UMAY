import SwiftUI
import AVFoundation
import UniformTypeIdentifiers

/// Okuyucu: yanıtı sistemin Türkçe sesiyle okur (Piper gerekmez).
/// "Dede Korkut tınısı" açıkken (yalnız macOS): konuşma `say` ile dosyaya yazılır, sonra BİZİM KENDİ kodumuzla
/// işlenir (yeniden örnekleme: perde aşağı ve %17 yavaş; süzgeçler: çello gibi sıcak orta bas; çok hafif yankı)
/// ve düz oynatıcıyla çalınır. Apple'ın ses birimleri (AVAudioUnit…) kullanılmaz: bu Mac'te onlar çöküyordu.
/// Okunan: yanıtın özü. "(Dayanağım…)", "Güven:" gibi künye satırları okunmaz, yazıda kalır.
final class Okuyucu {
    static let ortak = Okuyucu()
    private let ses = AVSpeechSynthesizer()
    private var calar: AVAudioPlayer?
    private var nesil = 0   // eski konuşmanın geç biten işini yok saymak için

    static func ozunuAyir(_ metin: String) -> String {
        let satirlar = metin.split(separator: "\n", omittingEmptySubsequences: true).map(String.init)
        let ozu = satirlar.filter { !($0.hasPrefix("(Dayanağım") || $0.hasPrefix("Güven:")
                                      || $0.hasPrefix("Dikkat:") || $0.hasPrefix("[")) }
        return ozu.joined(separator: " ")
    }

    private func soyleyici(_ metin: String) -> AVSpeechUtterance {
        let s = AVSpeechUtterance(string: metin)
        // Erkek Türkçe ses (Cem) varsa o; yoksa en kaliteli Türkçe ses.
        let sesler = AVSpeechSynthesisVoice.speechVoices().filter { $0.language == "tr-TR" }
        let erkek = sesler.filter { $0.name.contains("Cem") }
        s.voice = erkek.max(by: { $0.quality.rawValue < $1.quality.rawValue })
            ?? sesler.max(by: { $0.quality.rawValue < $1.quality.rawValue })
            ?? AVSpeechSynthesisVoice(language: "tr-TR")
        s.rate = AVSpeechUtteranceDefaultSpeechRate
        return s
    }

    private var konusuyor: Bool { ses.isSpeaking || (calar?.isPlaying ?? false) }

    private func sustur() {
        nesil += 1
        ses.stopSpeaking(at: .immediate)
        calar?.stop(); calar = nil
    }

    private func konus(_ metin: String) {
        sustur()
        let benim = nesil
        let soz = soyleyici(metin)
        #if os(macOS)
        if !UserDefaults.standard.bool(forKey: "umay.tinli") { ses.speak(soz); return }
        let sesAdi = soz.voice?.name ?? ""
        let gecici = FileManager.default.temporaryDirectory
        let ham = gecici.appendingPathComponent("umay-ham-\(benim).aiff")
        let son = gecici.appendingPathComponent("umay-tin-\(benim).wav")
        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            let ok = Okuyucu.sayUret(metin, ses: sesAdi, dosya: ham) && Okuyucu.tinla(ham, son)
            try? FileManager.default.removeItem(at: ham)
            DispatchQueue.main.async {
                guard let self = self, self.nesil == benim else { try? FileManager.default.removeItem(at: son); return }
                if ok, let o = try? AVAudioPlayer(contentsOf: son) {
                    self.calar = o
                    o.prepareToPlay(); o.play()
                } else {
                    self.ses.speak(soz)   // olmazsa düz sesle oku, çökme
                }
            }
        }
        #else
        ses.speak(soz)
        #endif
    }

    #if os(macOS)
    /// `say -v <ses> -o <dosya> -f -` ile metni AIFF'e sentezler (metin standart girişten gider).
    static func sayUret(_ metin: String, ses: String, dosya: URL) -> Bool {
        let p = Process()
        p.executableURL = URL(fileURLWithPath: "/usr/bin/say")
        var arg = ["-o", dosya.path, "--data-format=LEF32@22050", "-f", "-"]
        if !ses.isEmpty { arg = ["-v", ses] + arg }
        p.arguments = arg
        let giris = Pipe()
        p.standardInput = giris
        p.standardError = FileHandle.nullDevice
        p.standardOutput = FileHandle.nullDevice
        do { try p.run() } catch { return false }
        giris.fileHandleForWriting.write(metin.data(using: .utf8) ?? Data())
        try? giris.fileHandleForWriting.close()
        p.waitUntilExit()
        return p.terminationStatus == 0 && FileManager.default.fileExists(atPath: dosya.path)
    }

    // ---- kendi ses işleme kodumuz (Apple ses birimleri yok) ----
    private struct Suzgec {
        var b0: Double, b1: Double, b2: Double, a1: Double, a2: Double
        init(_ b0: Double, _ b1: Double, _ b2: Double, _ a0: Double, _ a1: Double, _ a2: Double) {
            self.b0 = b0 / a0; self.b1 = b1 / a0; self.b2 = b2 / a0; self.a1 = a1 / a0; self.a2 = a2 / a0
        }
        static func ustGecir(_ f: Double, _ fs: Double, q: Double = 0.707) -> Suzgec {   // high-pass
            let w = 2 * Double.pi * f / fs, c = cos(w), al = sin(w) / (2 * q)
            return Suzgec((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al)
        }
        static func tepe(_ f: Double, _ fs: Double, db: Double, q: Double) -> Suzgec {  // peaking
            let w = 2 * Double.pi * f / fs, c = cos(w), al = sin(w) / (2 * q), A = pow(10, db / 40)
            return Suzgec(1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A)
        }
        static func ustRaf(_ f: Double, _ fs: Double, db: Double) -> Suzgec {           // high-shelf
            let w = 2 * Double.pi * f / fs, c = cos(w), A = pow(10, db / 40)
            let al = sin(w) / 2 * 2.0.squareRoot(), be = 2 * A.squareRoot() * al
            return Suzgec(A * ((A + 1) + (A - 1) * c + be), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - be),
                          (A + 1) - (A - 1) * c + be, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - be)
        }
        func uygula(_ x: inout [Float]) {
            var x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0
            for i in 0..<x.count {
                let g = Double(x[i])
                let y = b0 * g + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
                x2 = x1; x1 = g; y2 = y1; y1 = y
                x[i] = Float(y)
            }
        }
    }

    /// Dede Korkut tınısı: yeniden örnekle (0,83: ~3 yarım ton aşağı, %17 yavaş), çello süzgeçleri, hafif yankı.
    static func tinla(_ girdi: URL, _ cikti: URL) -> Bool {
        guard let f = try? AVAudioFile(forReading: girdi), f.length > 0,
              let tampon = AVAudioPCMBuffer(pcmFormat: f.processingFormat, frameCapacity: AVAudioFrameCount(f.length)),
              (try? f.read(into: tampon)) != nil, let kanal = tampon.floatChannelData else { return false }
        let n = Int(tampon.frameLength)
        let fs = f.processingFormat.sampleRate
        let giris = Array(UnsafeBufferPointer(start: kanal[0], count: n))
        // 1) yeniden örnekleme (doğrusal aradeğerleme)
        let oran = 0.83
        let m = Int(Double(n) / oran)
        var x = [Float](repeating: 0, count: m)
        for i in 0..<m {
            let k = Double(i) * oran, j = Int(k), t = Float(k - Double(j))
            let a = giris[min(j, n - 1)], b = giris[min(j + 1, n - 1)]
            x[i] = a + (b - a) * t
        }
        // 2) çello gibi sıcak orta bas
        Suzgec.ustGecir(70, fs).uygula(&x)
        Suzgec.tepe(220, fs, db: 6, q: 1.2).uygula(&x)
        Suzgec.tepe(450, fs, db: 3, q: 1.0).uygula(&x)
        Suzgec.tepe(2800, fs, db: 3, q: 1.0).uygula(&x)
        Suzgec.ustRaf(7000, fs, db: -4).uygula(&x)
        // 3) çok hafif yankı (55 ms)
        let gec = Int(0.055 * fs)
        var y = x
        if gec < m { for i in gec..<m { y[i] = 0.9 * x[i] + 0.07 * x[i - gec] } }
        // 4) sesi doruğa göre ayarla
        let doruk = y.map { abs($0) }.max() ?? 1
        let kazanc: Float = doruk > 0 ? 0.85 / doruk : 1
        for i in 0..<m { y[i] *= kazanc }
        // 5) yaz
        guard let bicim = AVAudioFormat(standardFormatWithSampleRate: fs, channels: 1),
              let cikis = AVAudioPCMBuffer(pcmFormat: bicim, frameCapacity: AVAudioFrameCount(m)),
              let hedef = cikis.floatChannelData else { return false }
        for i in 0..<m { hedef[0][i] = y[i] }
        cikis.frameLength = AVAudioFrameCount(m)
        guard let dosya = try? AVAudioFile(forWriting: cikti, settings: bicim.settings) else { return false }
        return (try? dosya.write(from: cikis)) != nil
    }
    #endif

    /// Düğmeyle: konuşuyorsa susturur, susuyorsa okur.
    func oku(_ metin: String) {
        if konusuyor { sustur(); return }
        let ozu = Okuyucu.ozunuAyir(metin)
        guard !ozu.isEmpty else { return }
        konus(ozu)
    }

    /// Yanıt gelince kendiliğinden: önceki konuşmayı keser, yenisini okur.
    func kendiliginden(_ metin: String) {
        let ozu = Okuyucu.ozunuAyir(metin)
        guard !ozu.isEmpty else { return }
        konus(ozu)
    }
}

struct SohbetGorunumu: View {
    @StateObject private var motor = UmayMotor()
    @State private var girdi = ""
    @State private var ayarAcik = false
    @State private var belgelerAcik = false
    @State private var belgeSec = false
    @AppStorage("umay.belgeAlani") private var belgeAlani = "otomatik"
    @Environment(\.scenePhase) private var evre

    static let belgeAlanlari: [(String, String)] = [
        ("otomatik", "Otomatik (ajanlar karar verir)"),
        ("yazit", "Yazıt — kültür, tarih, genel arşiv"),
        ("yer-su", "Yer-Su — tarım, toprak, su"),
        ("balbal", "Balbal — yapı, taş ev, mimari"),
        ("kun", "Kün — güneş enerjisi"),
        ("demir", "Demir — elektronik, donanım"),
        ("tulga", "Tulga — araç, otomotiv"),
        ("otaci", "Otacı — sağlık, tıp"),
        ("yargu", "Yargu — hukuk"),
        ("akca", "Akça — para, ekonomi"),
        ("alpagu", "Alpagu — güvenlik, savunma"),
        ("kopuz", "Kopuz — müzik"),
        ("oyun", "Oyun — dans"),
    ]

    var body: some View {
        VStack(spacing: 0) {
            ustSerit
            Divider()
            ScrollViewReader { vekil in
                ScrollView {
                    LazyVStack(alignment: .leading, spacing: 14) {
                        if motor.iletiler.isEmpty { karsilama }
                        ForEach(motor.iletiler) { i in ileti(i).textSelection(.enabled).frame(maxWidth: .infinity, alignment: .leading) }
                        if motor.mesgul { ProgressView("Düşünüyor…") }
                        Color.clear.frame(height: 1).id("son")
                    }
                    .padding()
                }
                .onChange(of: motor.iletiler.count) { _ in
                    withAnimation { vekil.scrollTo("son", anchor: .bottom) }
                }
            }
            Divider()
            HStack {
                Button { belgeSec = true } label: { Image(systemName: "doc.badge.plus") }
                    .help("Belge ekle: ajanlar inceleyip yerleştirir")
                Menu {
                    Picker("Eklenen belgenin alanı", selection: $belgeAlani) {
                        ForEach(Self.belgeAlanlari, id: \.0) { Text($0.1).tag($0.0) }
                    }
                } label: { Text(belgeAlani == "otomatik" ? "Alan: otomatik" : "Alan: \(belgeAlani)").font(.caption) }
                .menuStyle(.borderlessButton).fixedSize()
                TextField("Bir şey sor…", text: $girdi)
                    .textFieldStyle(.roundedBorder)
                    .onSubmit(gonder)
                Button("Sor", action: gonder)
                    .disabled(girdi.trimmingCharacters(in: .whitespaces).isEmpty || motor.mesgul)
            }
            .padding()
        }
        .onChange(of: evre) { yeni in if yeni == .active { motor.gelenYokla() } }
        .sheet(isPresented: $ayarAcik) { AyarGorunumu(motor: motor) }
        .sheet(isPresented: $belgelerAcik) { BelgeGorunumu(motor: motor) }
        .fileImporter(isPresented: $belgeSec,
                      allowedContentTypes: [.plainText, .pdf, UTType(filenameExtension: "md") ?? .plainText],
                      allowsMultipleSelection: true) { sonuc in
            if case .success(let adresler) = sonuc { motor.belgeEkle(adresler, alan: belgeAlani == "otomatik" ? "" : belgeAlani) }
        }
        #if os(macOS)
        .frame(minWidth: 460, minHeight: 560)
        #endif
    }

    private var ustSerit: some View {
        HStack {
            VStack(alignment: .leading, spacing: 2) {
                Text("UMAY").font(.headline)
                // Dürüst durum: yedek motor gerçek dil modeli DEĞİLDİR.
                Text(motor.gercekModel ? "Dil modeli: \(motor.modelDosyasi)" : "Yedek motor — dil modeli yok (Ayarlar'dan indirin)")
                    .font(.caption).foregroundColor(motor.gercekModel ? .secondary : .orange)
            }
            Spacer()
            Button { if let y = motor.iletiler.last(where: { !$0.benMi && !$0.sistemMi }) { Okuyucu.ortak.oku(y.metin) } } label: { Image(systemName: "speaker.wave.2") }
                .help("Son yanıtı dinle")
                .disabled(!motor.iletiler.contains(where: { !$0.benMi && !$0.sistemMi }))
            Button { Panoya.kopyala(motor.konusmaMetni()) } label: { Image(systemName: "doc.on.doc") }
                .help("Bütün konuşmayı kopyala")
                .disabled(motor.iletiler.isEmpty)
            Button { motor.temizle() } label: { Image(systemName: "trash") }
                .disabled(motor.iletiler.isEmpty || motor.mesgul)
            Button { belgelerAcik = true } label: { Image(systemName: "books.vertical") }
                .help("Belgelerim: alanını değiştir ya da kaldır")
            Button { ayarAcik = true } label: { Image(systemName: "gearshape") }
        }
        .padding(.horizontal).padding(.vertical, 8)
    }

    private var karsilama: some View {
        Text("Merhaba, ben Dede Korkut. Yer altı kileri, permakültür, güneş enerjisi, kerpiç yapı… ne merak ediyorsan sor.")
            .foregroundColor(.secondary).padding(.top, 20)
    }

    /// Balonsuz ileti: adı renkli, altında metin. (Tek dev metin kaydırmayı bozduğu için iletiler ayrı.)
    private func ileti(_ i: UmayMotor.Ileti) -> Text {
        if i.benMi { return Text(i.metin).foregroundColor(.accentColor) }
        let ad = i.sistemMi ? "UMAY" : "Dede Korkut"
        let renk: Color = i.sistemMi ? .orange : .secondary
        return Text(ad + "\n").bold().foregroundColor(renk) + Text(i.metin)
    }

    private func gonder() {
        let s = girdi
        girdi = ""
        motor.sor(s)
    }
}

/// Panoya yazma (macOS ve iOS).
enum Panoya {
    static func kopyala(_ metin: String) {
        #if os(macOS)
        NSPasteboard.general.clearContents()
        NSPasteboard.general.setString(metin, forType: .string)
        #else
        UIPasteboard.general.string = metin
        #endif
    }
}

extension UmayMotor {
    /// Bütün konuşma düz metin: "Sen: …" / "UMAY: …".
    func konusmaMetni() -> String {
        iletiler.map { ($0.benMi ? "Hakan: " : ($0.sistemMi ? "UMAY: " : "Dede Korkut: ")) + $0.metin }
            .joined(separator: "\n\n")
    }
}

import SwiftUI
import AVFoundation
import UniformTypeIdentifiers

/// Okuyucu: yanıtı sistemin Türkçe sesiyle okur (Piper gerekmez), sonra Dede Korkut tınısına çevirir:
/// perde ~3,4 yarım ton aşağı, %15 yavaş, çello gibi sıcak orta bas, çok hafif yankı.
/// Okunan: yanıtın özü. "(Dayanağım…)", "Güven:" gibi künye satırları okunmaz, yazıda kalır.
final class Okuyucu {
    static let ortak = Okuyucu()
    private let ses = AVSpeechSynthesizer()
    private let motor = AVAudioEngine()
    private let oynatici = AVAudioPlayerNode()
    private let perde = AVAudioUnitTimePitch()
    private let esitleyici = AVAudioUnitEQ(numberOfBands: 4)
    private let yanki = AVAudioUnitReverb()
    private var bagliBicim: AVAudioFormat?
    private var konusuyor = false
    private var nesil = 0   // eski konuşmanın geç gelen tamponlarını yok saymak için

    init() {
        perde.pitch = -343        // sent: yaklaşık -3,4 yarım ton
        perde.rate = 0.85         // %15 yavaş
        let b = esitleyici.bands
        b[0].filterType = .highPass;  b[0].frequency = 70;   b[0].bypass = false
        b[1].filterType = .parametric; b[1].frequency = 220;  b[1].bandwidth = 1.2; b[1].gain = 6; b[1].bypass = false
        b[2].filterType = .parametric; b[2].frequency = 450;  b[2].bandwidth = 1.5; b[2].gain = 3; b[2].bypass = false
        b[3].filterType = .highShelf;  b[3].frequency = 7000; b[3].gain = -4;       b[3].bypass = false
        yanki.loadFactoryPreset(.smallRoom)
        yanki.wetDryMix = 7       // çok az yankı
        [oynatici, perde, esitleyici, yanki].forEach { motor.attach($0) }
    }

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

    private func zincirKur(_ bicim: AVAudioFormat) {
        if let b = bagliBicim, b == bicim { return }
        if motor.isRunning { motor.stop() }
        motor.disconnectNodeOutput(oynatici); motor.disconnectNodeOutput(perde)
        motor.disconnectNodeOutput(esitleyici); motor.disconnectNodeOutput(yanki)
        motor.connect(oynatici, to: perde, format: bicim)
        motor.connect(perde, to: esitleyici, format: bicim)
        motor.connect(esitleyici, to: yanki, format: bicim)
        motor.connect(yanki, to: motor.mainMixerNode, format: bicim)
        bagliBicim = bicim
    }

    private func sustur() {
        nesil += 1
        ses.stopSpeaking(at: .immediate)
        oynatici.stop()
        konusuyor = false
    }

    private func konus(_ metin: String) {
        sustur()
        let benim = nesil
        konusuyor = true
        ses.write(soyleyici(metin)) { [weak self] tampon in
            guard let self = self, self.nesil == benim,
                  let pcm = tampon as? AVAudioPCMBuffer, pcm.frameLength > 0 else { return }
            DispatchQueue.main.async {
                guard self.nesil == benim else { return }
                self.zincirKur(pcm.format)
                if !self.motor.isRunning { try? self.motor.start() }
                if !self.oynatici.isPlaying { self.oynatici.play() }
                self.oynatici.scheduleBuffer(pcm, completionHandler: nil)
            }
        }
        // Konuşma süresi kestirilemediğinden "konuşuyor" bayrağı yeni komutla ya da durdurmayla sıfırlanır.
    }

    /// Düğmeyle: konuşuyorsa susturur, susuyorsa okur.
    func oku(_ metin: String) {
        if konusuyor && oynatici.isPlaying { sustur(); return }
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

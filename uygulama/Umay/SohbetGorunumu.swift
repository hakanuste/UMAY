import SwiftUI
import AVFoundation
import UniformTypeIdentifiers

/// Okuyucu: yanıtı sistemin Türkçe sesiyle okur (Piper gerekmez).
final class Okuyucu {
    static let ortak = Okuyucu()
    private let ses = AVSpeechSynthesizer()
    func oku(_ metin: String) {
        if ses.isSpeaking { ses.stopSpeaking(at: .immediate); return }
        let s = AVSpeechUtterance(string: metin)
        s.voice = AVSpeechSynthesisVoice(language: "tr-TR")
        ses.speak(s)
    }
}

struct SohbetGorunumu: View {
    @StateObject private var motor = UmayMotor()
    @State private var girdi = ""
    @State private var ayarAcik = false
    @State private var belgeSec = false

    var body: some View {
        VStack(spacing: 0) {
            ustSerit
            Divider()
            ScrollViewReader { vekil in
                ScrollView {
                    LazyVStack(alignment: .leading, spacing: 10) {
                        if motor.iletiler.isEmpty { karsilama }
                        ForEach(motor.iletiler) { i in baloncuk(i).id(i.id) }
                        if motor.mesgul { ProgressView("Düşünüyor…").padding(.leading) }
                    }
                    .padding()
                }
                .onChange(of: motor.iletiler.count) { _ in
                    if let son = motor.iletiler.last { withAnimation { vekil.scrollTo(son.id, anchor: .bottom) } }
                }
            }
            Divider()
            HStack {
                Button { belgeSec = true } label: { Image(systemName: "doc.badge.plus") }
                    .help("Belge ekle: ajanlar inceleyip yerleştirir")
                TextField("Bir şey sor…", text: $girdi)
                    .textFieldStyle(.roundedBorder)
                    .onSubmit(gonder)
                Button("Sor", action: gonder)
                    .disabled(girdi.trimmingCharacters(in: .whitespaces).isEmpty || motor.mesgul)
            }
            .padding()
        }
        .sheet(isPresented: $ayarAcik) { AyarGorunumu(motor: motor) }
        .fileImporter(isPresented: $belgeSec,
                      allowedContentTypes: [.plainText, .pdf, UTType(filenameExtension: "md") ?? .plainText],
                      allowsMultipleSelection: true) { sonuc in
            if case .success(let adresler) = sonuc { motor.belgeEkle(adresler) }
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
            Button { motor.temizle() } label: { Image(systemName: "trash") }
                .disabled(motor.iletiler.isEmpty || motor.mesgul)
            Button { ayarAcik = true } label: { Image(systemName: "gearshape") }
        }
        .padding(.horizontal).padding(.vertical, 8)
    }

    private var karsilama: some View {
        Text("Merhaba, ben Dede Korkut. Yer altı kileri, permakültür, güneş enerjisi, kerpiç yapı… ne merak ediyorsan sor.")
            .foregroundColor(.secondary).padding(.top, 20)
    }

    private func baloncuk(_ i: UmayMotor.Ileti) -> some View {
        HStack(alignment: .top) {
            if i.benMi { Spacer(minLength: 40) }
            VStack(alignment: .leading, spacing: 4) {
                Text(i.metin).textSelection(.enabled)
                if !i.benMi && !i.sistemMi {
                    Button { Okuyucu.ortak.oku(i.metin) } label: { Label("Dinle", systemImage: "speaker.wave.2") }
                        .font(.caption).buttonStyle(.borderless)
                }
            }
            .padding(10)
            .background(i.benMi ? Color.accentColor.opacity(0.18) : Color.gray.opacity(0.15))
            .cornerRadius(12)
            if !i.benMi { Spacer(minLength: 40) }
        }
    }

    private func gonder() {
        let s = girdi
        girdi = ""
        motor.sor(s)
    }
}

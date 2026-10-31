import SwiftUI
import UniformTypeIdentifiers
#if os(macOS)
import AppKit
#endif

/// KASA: parolalar, sağlık ve arazi kütükleri için şifreli özel kasa. Ana parola SOHBETE değil, yalnız bu pencereye yazılır.
final class KasaDurumu: ObservableObject {
    struct Kayit: Identifiable { let id: String; let tur: Int32; let boy: UInt32 }

    @Published var kuruldu = false
    @Published var acik = false
    @Published var kayitlar: [Kayit] = []
    @Published var ileti = ""
    @Published var mesgul = false
    @Published var gorunenAd = ""
    @Published var gorunenMetin = ""

    private let kuyruk = DispatchQueue(label: "umay.kasa")
    private var zamanlayici: Timer?

    init() {
        yenile()
        // Çekirdek 5 dakika boşta kalınca kendiliğinden kilitler; ekran da bunu 20 sn'de bir öğrenir.
        zamanlayici = Timer.scheduledTimer(withTimeInterval: 20, repeats: true) { [weak self] _ in self?.yenile() }
    }
    deinit { zamanlayici?.invalidate() }

    static func hataMetni(_ kod: Int32) -> String {
        switch kod {
        case -1: return "Kasa ya da kayıt bulunamadı."
        case -2: return "Parola yanlış ya da kasa kütüğü değiştirilmiş."
        case -3: return "Bu aygıtta güvenli rastgele kaynağı yok; kasa kullanılamaz."
        case -4: return "Bellek ya da yer yetmedi (kasa en çok 8 MB tutar)."
        case -5: return "Parola çok kısa (en az 10 karakter)."
        case -6: return "Kasa kilitli."
        default: return "İşlem olmadı (\(kod))."
        }
    }

    func yenile() {
        kuyruk.async { [weak self] in
            let k = umay_gomulu_kasa_var() != 0
            let a = umay_gomulu_kasa_acik() != 0
            var liste: [Kayit] = []
            if a {
                let n = umay_gomulu_kasa_say()
                if n > 0 {
                    for i in 0..<Int(n) {
                        var ad = [CChar](repeating: 0, count: 80)
                        var tur: Int32 = 0
                        var boy: UInt32 = 0
                        if umay_gomulu_kasa_bilgi(Int32(i), &ad, UInt32(ad.count), &tur, &boy) == 0 {
                            liste.append(Kayit(id: String(cString: ad), tur: tur, boy: boy))
                        }
                    }
                }
            }
            DispatchQueue.main.async {
                guard let self = self else { return }
                self.kuruldu = k
                self.acik = a
                self.kayitlar = liste
                if !a { self.gorunenAd = ""; self.gorunenMetin = "" }
            }
        }
    }

    private func calistir(_ is_: @escaping () -> Int32, tamam: String) {
        mesgul = true; ileti = ""
        kuyruk.async { [weak self] in
            let kod = is_()
            DispatchQueue.main.async {
                guard let self = self else { return }
                self.mesgul = false
                self.ileti = kod == 0 ? tamam : KasaDurumu.hataMetni(kod)
                self.yenile()
            }
        }
    }

    func kur(_ parola: String) { calistir({ umay_gomulu_kasa_kur(parola) }, tamam: "Kasa kuruldu. Yedek almayı unutma: kasa.bin ve kasa.yed.") }
    func ac(_ parola: String) { calistir({ umay_gomulu_kasa_ac(parola) }, tamam: "") }
    func kilitle() { kuyruk.async { umay_gomulu_kasa_kilitle() }; yenile() }

    func ekle(ad: String, tur: Int32, veri: Data) {
        let temiz = String(ad.prefix(40))
        calistir({
            veri.withUnsafeBytes { p in umay_gomulu_kasa_yaz(temiz, tur, p.baseAddress, UInt32(p.count)) }
        }, tamam: "'\(temiz)' kasaya eklendi (şifreli).")
    }

    func sil(_ ad: String) { calistir({ umay_gomulu_kasa_sil(ad) }, tamam: "'\(ad)' silindi.") }

    /// Kaydı bellekten okur (kilit açıkken). Metni pencerede gösterir; dosyaysa kaydetme paneli açar.
    func ac(kayit: Kayit) {
        kuyruk.async { [weak self] in
            let boy = umay_gomulu_kasa_boy(kayit.id)
            guard boy >= 0 else { return }
            var tampon = [UInt8](repeating: 0, count: Int(boy) + 1)
            let g = umay_gomulu_kasa_oku(kayit.id, &tampon, UInt32(boy))
            guard g >= 0 else { return }
            let veri = Data(tampon.prefix(Int(g)))
            DispatchQueue.main.async {
                guard let self = self else { return }
                if kayit.tur == 0 {
                    self.gorunenAd = kayit.id
                    self.gorunenMetin = String(decoding: veri, as: UTF8.self)
                } else {
                    #if os(macOS)
                    let panel = NSSavePanel()
                    panel.nameFieldStringValue = kayit.id
                    panel.message = "Düz (şifresiz) hâliyle kaydedilir. İşin bitince o dosyayı sil."
                    if panel.runModal() == .OK, let u = panel.url {
                        do { try veri.write(to: u); self.ileti = "'\(kayit.id)' dışarı çıkarıldı." }
                        catch { self.ileti = "Yazılamadı: \(error.localizedDescription)" }
                    }
                    #endif
                }
            }
        }
    }
}

struct KasaGorunumu: View {
    @StateObject private var d = KasaDurumu()
    @Environment(\.dismiss) private var kapat
    @State private var parola = ""
    @State private var parola2 = ""
    @State private var dosyaSec = false
    @State private var notAdi = ""
    @State private var notMetni = ""
    @State private var notAcik = false

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack {
                Image(systemName: d.acik ? "lock.open" : "lock.shield").font(.title2)
                Text("Kasa").font(.title2.bold())
                Spacer()
                if d.acik { Button("Kilitle") { d.kilitle() } }
                Button("Kapat") { d.kilitle(); kapat() }
            }
            Text("Parolalar, sağlık ve arazi kütükleri burada şifreli durur. Ana parola yalnız bu pencereye yazılır; sohbete, modele ve ağa hiç gitmez.")
                .font(.caption).foregroundColor(.secondary)
            Divider()
            if !d.kuruldu { kurma } else if !d.acik { kilit } else { icerik }
            if !d.ileti.isEmpty { Text(d.ileti).font(.callout).foregroundColor(.orange) }
            Spacer(minLength: 0)
            Text("Ana parolayı unutursan içerik KURTARILAMAZ (arka kapı yoktur). 5 dakika boşta kalınca kendiliğinden kilitlenir.")
                .font(.caption2).foregroundColor(.secondary)
        }
        .padding()
        #if os(macOS)
        .frame(minWidth: 480, minHeight: 520)
        #endif
        .fileImporter(isPresented: $dosyaSec, allowedContentTypes: [.data], allowsMultipleSelection: false) { sonuc in
            guard case .success(let adresler) = sonuc, let a = adresler.first else { return }
            let erisim = a.startAccessingSecurityScopedResource()
            defer { if erisim { a.stopAccessingSecurityScopedResource() } }
            if let veri = try? Data(contentsOf: a) { d.ekle(ad: a.lastPathComponent, tur: 1, veri: veri) }
            else { d.ileti = "Dosya okunamadı." }
        }
    }

    private var kurma: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("Henüz kasa yok. Ana parola belirle (en az 10 karakter).")
            SecureField("Ana parola", text: $parola).textFieldStyle(.roundedBorder)
            SecureField("Yinele", text: $parola2).textFieldStyle(.roundedBorder)
            Button("Kasayı kur") {
                guard parola == parola2 else { d.ileti = "Parolalar uyuşmuyor."; return }
                d.kur(parola); parola = ""; parola2 = ""
            }
            .disabled(d.mesgul || parola.count < 10)
        }
    }

    private var kilit: some View {
        VStack(alignment: .leading, spacing: 8) {
            SecureField("Ana parola", text: $parola).textFieldStyle(.roundedBorder).onSubmit(ac)
            Button("Aç", action: ac).disabled(d.mesgul || parola.isEmpty)
            if d.mesgul { ProgressView("Açılıyor…") }
        }
    }
    private func ac() { d.ac(parola); parola = "" }

    private var icerik: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack {
                Button { dosyaSec = true } label: { Label("Dosya ekle", systemImage: "doc.badge.plus") }
                Button { notAcik.toggle() } label: { Label("Not ekle", systemImage: "square.and.pencil") }
            }
            if notAcik {
                TextField("Notun adı (örn. tapu-bilgileri)", text: $notAdi).textFieldStyle(.roundedBorder)
                TextEditor(text: $notMetni).frame(minHeight: 90).border(Color.secondary.opacity(0.4))
                Button("Notu kasaya kaydet") {
                    d.ekle(ad: notAdi, tur: 0, veri: Data(notMetni.utf8))
                    notAdi = ""; notMetni = ""; notAcik = false
                }
                .disabled(notAdi.trimmingCharacters(in: .whitespaces).isEmpty || notMetni.isEmpty)
            }
            if d.kayitlar.isEmpty { Text("Kasa boş.").foregroundColor(.secondary) }
            List(d.kayitlar) { k in
                HStack {
                    Image(systemName: k.tur == 0 ? "note.text" : "doc")
                    VStack(alignment: .leading) {
                        Text(k.id)
                        Text("\(k.tur == 0 ? "not" : "dosya") · \(k.boy / 1024) KB").font(.caption).foregroundColor(.secondary)
                    }
                    Spacer()
                    Button(k.tur == 0 ? "Göster" : "Dışarı çıkar") { d.ac(kayit: k) }
                    Button(role: .destructive) { d.sil(k.id) } label: { Image(systemName: "trash") }
                }
            }
            .frame(minHeight: 130)
            if !d.gorunenAd.isEmpty {
                GroupBox(d.gorunenAd) {
                    ScrollView { Text(d.gorunenMetin).textSelection(.enabled).frame(maxWidth: .infinity, alignment: .leading) }
                        .frame(maxHeight: 160)
                }
                Button("Gizle") { d.gorunenAd = ""; d.gorunenMetin = "" }
            }
        }
    }
}

import SwiftUI

/// Pencerenin sağ yanında açılan pano. Sohbet hep ortada kalır; panolar yanına dizilir.
enum SagPano: String {
    case yok, ajanlar, belgeler, kasa
}

/// Ana pencere (macOS): sol kenar şeridi | sohbet | isteğe bağlı sağ pano. Hepsi tek pencerenin içinde.
/// (HSplitView macOS 10.15'ten beri vardır; NavigationSplitView macOS 13 ister, bu yüzden kullanılmadı.)
struct AnaGorunum: View {
    @StateObject private var motor = UmayMotor()
    @State private var sag: SagPano = .yok

    var body: some View {
        #if os(macOS)
        HSplitView {
            KenarSeridi(sag: $sag, motor: motor)
                .frame(minWidth: 170, idealWidth: 190, maxWidth: 250)
            SohbetGorunumu(motor: motor, sag: $sag)
                .frame(minWidth: 460)
            if sag != .yok {
                sagPano
                    .frame(minWidth: 320, idealWidth: 420)
            }
        }
        #else
        SohbetGorunumu(motor: motor, sag: $sag)
        #endif
    }

    @ViewBuilder private var sagPano: some View {
        switch sag {
        case .ajanlar: AjanlarGorunumu(motor: motor) { sag = .yok }
        case .belgeler: BelgeGorunumu(motor: motor, kapatEylemi: { sag = .yok })
        case .kasa: KasaGorunumu(kapatEylemi: { sag = .yok })
        case .yok: EmptyView()
        }
    }
}

/// Sol şerit: panolar arasında geçiş.
struct KenarSeridi: View {
    @Binding var sag: SagPano
    @ObservedObject var motor: UmayMotor

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            HStack(spacing: 8) {
                if let a = SohbetGorunumu.resim("umay-yuz") {
                    a.resizable().interpolation(.high).antialiased(true).scaledToFit().frame(width: 36, height: 36)
                }
                Text("UMAY").font(.headline)
            }
            .padding(.bottom, 10)
            satir("Sohbet", "bubble.left.and.bubble.right", "dede-korkut-yuz", .yok)
            satir("Ajanlar", "person.3", "umay-yuz", .ajanlar)
            satir("Belgelerim", "books.vertical", "amblem-bitig", .belgeler)
            satir("Kasa", "lock.shield", "amblem-tamga", .kasa)
            Spacer()
            Text(motor.mesgul ? "Düşünüyor…" : motor.durum)
                .font(.caption2).foregroundColor(.secondary).lineLimit(2)
        }
        .padding(10)
        .frame(maxHeight: .infinity, alignment: .top)
    }

    private func satir(_ ad: String, _ simge: String, _ resim: String, _ p: SagPano) -> some View {
        Button { sag = (sag == p && p != .yok) ? .yok : p } label: {
            HStack(spacing: 8) {
                if let r = SohbetGorunumu.resim(resim) {
                    r.resizable().interpolation(.high).antialiased(true).scaledToFit().frame(width: 28, height: 28)
                } else {
                    Image(systemName: simge).frame(width: 28)
                }
                Text(ad)
                Spacer()
            }
            .padding(.vertical, 6).padding(.horizontal, 8)
            .background(sag == p ? Color.accentColor.opacity(0.18) : Color.clear)
            .cornerRadius(6)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
    }
}

/// Ajan → amblem dosyası (paketteki PNG adı).
enum AjanResmi {
    static let alan: [String: String] = [
        "bilge": "dede-korkut-amblem", "kabul": "amblem-kervan",
        "yazit": "amblem-bitig", "yer-su": "amblem-ergenekon", "balbal": "amblem-orkun", "kun": "amblem-ulgen",
        "demir": "amblem-ulak", "tulga": "amblem-tulpar", "otaci": "amblem-asena", "yargu": "amblem-tore",
        "akca": "amblem-kut", "alpagu": "amblem-timur", "kopuz": "dede-korkut-yuz", "oyun": "umay-amblem",
    ]
    @ViewBuilder static func goster(_ anahtar: String, boy: CGFloat = 44) -> some View {
        if let ad = alan[anahtar], let r = SohbetGorunumu.resim(ad) {
            r.resizable().interpolation(.high).antialiased(true).scaledToFit().frame(width: boy, height: boy)
        } else {
            Image(systemName: "person.crop.circle").font(.title2).frame(width: boy, height: boy)
        }
    }
}

/// Ajanlar panosu: alan uzmanları, kaç belge okudukları ve yeni belgenin hangisine gideceği.
struct AjanlarGorunumu: View {
    @ObservedObject var motor: UmayMotor
    var kapat: () -> Void
    @AppStorage("umay.belgeAlani") private var belgeAlani = "otomatik"
    @State private var sayilar: [String: Int] = [:]

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack {
                Text("Ajanlar").font(.title2).bold()
                Spacer()
                Button("Kapat", action: kapat)
            }
            Text("Her soruya alanına göre bir uzman bakar. Birden çok alanı ilgilendiren soruda BİLGE uzmanları bir araya toplar. Aşağıdan, eklediğin yeni belgenin hangi uzmana gideceğini seçebilirsin.")
                .font(.caption).foregroundColor(.secondary)
            ScrollView {
                VStack(alignment: .leading, spacing: 8) {
                    cekirdek("bilge", "BİLGE", "Bilgi bulma ve öğüt. Kütüphanede kayıt yoksa internetten arar; sen \"doğru, ekle\" dersen kaydeder.")
                    cekirdek("kabul", "KABUL", "Eklenen belgeyi inceler, alanını bulur ve kütüphaneye yerleştirir.")
                    Divider()
                    ForEach(SohbetGorunumu.belgeAlanlari.filter { $0.0 != "otomatik" }, id: \.0) { a in uzman(a) }
                }
            }
        }
        .padding()
        .onAppear(perform: say)
        .onChange(of: motor.mesgul) { _ in say() }
    }

    private func say() {
        var s: [String: Int] = [:]
        for b in UmayMotor.belgeleriListele() { s[b.alan.isEmpty ? "otomatik" : b.alan, default: 0] += 1 }
        sayilar = s
    }

    private func cekirdek(_ anahtar: String, _ ad: String, _ nitelik: String) -> some View {
        HStack(spacing: 10) {
            AjanResmi.goster(anahtar)
            VStack(alignment: .leading, spacing: 2) {
                Text(ad).bold()
                Text(nitelik).font(.caption).foregroundColor(.secondary)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(8)
        .background(Color.gray.opacity(0.12)).cornerRadius(6)
    }

    private func uzman(_ a: (String, String)) -> some View {
        let parca = a.1.components(separatedBy: " — ")
        let ad = parca.first ?? a.1
        let alan = parca.count > 1 ? parca[1] : ""
        let secili = belgeAlani == a.0
        return HStack(spacing: 10) {
            AjanResmi.goster(a.0)
            VStack(alignment: .leading, spacing: 2) {
                Text(ad).bold()
                Text(alan).font(.caption).foregroundColor(.secondary)
                Text("\(sayilar[a.0] ?? 0) elle yerleştirilmiş belge").font(.caption2).foregroundColor(.secondary)
            }
            Spacer()
            Button(secili ? "Seçili" : "Belge buna gitsin") { belgeAlani = secili ? "otomatik" : a.0 }
                .font(.caption)
        }
        .padding(8)
        .background(secili ? Color.accentColor.opacity(0.18) : Color.gray.opacity(0.12)).cornerRadius(6)
    }
}

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
        "bilge": "dede-korkut-amblem", "kabul": "amblem-kervan", "arastirmaci": "amblem-ulak", "cevirmen": "amblem-tulpar",
        "gozcu": "amblem-timur", "onarici": "amblem-ergenekon", "yazman": "amblem-bitig",
        "yazit": "amblem-bitig", "yer-su": "amblem-ergenekon", "balbal": "amblem-orkun", "kun": "amblem-ulgen",
        "demir": "amblem-ulak", "tulga": "amblem-tulpar", "otaci": "amblem-asena", "yargu": "amblem-tore",
        "akca": "amblem-kut", "alpagu": "amblem-timur", "kopuz": "dede-korkut-yuz", "oyun": "umay-amblem",
    ]
    /// Ajan adını eşleme anahtarına çevirir: küçültür ve Türkçe harfleri
    /// ASCII'ye katlar.
    ///
    /// NEDEN GEREKLİ: eşlemenin anahtarları ASCII ("arastirmaci",
    /// "gozcu", "onarici", "cevirmen") ama ajan adları Türkçe
    /// ("ARAŞTIRMACI", "GÖZCÜ", "ONARICI", "ÇEVİRMEN"). Yalnız
    /// `lowercased()` kullanılsa altı genel görevlinin DÖRDÜ
    /// eşleşmez ve amblem yerine genel kişi simgesi görünürdü.
    /// `folding(options: .diacriticInsensitive)` ı ve ş harflerini
    /// güvenilir biçimde katlamadığı için eşleme elle yapılıyor.
    static func anahtarla(_ ad: String) -> String {
        var s = ad.lowercased()
        for (a, b) in [("ç","c"),("ğ","g"),("ı","i"),("i̇","i"),("ö","o"),("ş","s"),("ü","u")] {
            s = s.replacingOccurrences(of: a, with: b)
        }
        return s
    }

    @ViewBuilder static func goster(_ anahtar: String, boy: CGFloat = 44) -> some View {
        if let ad = alan[anahtar], let r = SohbetGorunumu.resim(ad) {
            r.resizable().interpolation(.high).antialiased(true).scaledToFit().frame(width: boy, height: boy)
        } else {
            Image(systemName: "person.crop.circle").font(.title2).frame(width: boy, height: boy)
        }
    }
}

/// Ajanlar panosu — CANLI.
///
/// ÖNCESİ: bu pano bir TANITIM LİSTESİYDİ. 19 ajanın adı, işi ve kaç
/// belge okuduğu yazıyordu; hangisinin koştuğu, hangi göreve baktığı
/// hiç görünmüyordu. Kullanıcı ajanların çalıştığını ancak sohbetin
/// içinde, araştırma çıktısının arasında görebiliyordu.
///
/// SEBEBİ: canlı durum çekirdekte VARDI (her ajanın durumu, görev türü
/// maskesi, canlı görevi, başlangıç anı), ama köprüden geçmiyordu —
/// Swift hangi ajanın çalıştığını öğrenemiyordu bile. Köprü açıldı
/// (umay_gomulu_ajan_say / umay_gomulu_ajan_bilgi) ve pano artık işi
/// gösteriyor.
///
/// NEDEN AÇILIR PENCERE DEĞİL: kendiliğinden açılan bir pencere, sen
/// yazarken odağı çalar ve kapandığında kayıt kaybolur. Ayrıca iOS'ta
/// pencere diye bir şey yoktur; iPhone da hedef olduğu için macOS'a
/// özgü bir açılır pencere arayüzü ikiye bölerdi. Onun yerine: pano
/// burada durur, istersen macOS'ta "Kendi penceresinde aç" ile
/// ayırırsın — senin açıp açık bıraktığın bir pencere olur.
struct AjanlarGorunumu: View {
    @ObservedObject var motor: UmayMotor
    var kapat: () -> Void
    /// Ayrı pencerede gösterilirken "Kapat" ve "Ayır" düğmeleri gizlenir.
    var penceredeMi = false
    @AppStorage("umay.belgeAlani") private var belgeAlani = "otomatik"
    @State private var sayilar: [String: Int] = [:]
    @State private var saat: Timer?

    private var calisanlar: [UmayMotor.Ajan] { motor.ajanlar.filter { $0.calisiyorMu } }

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            if !penceredeMi {
                HStack {
                    Text("Ajanlar").font(.title2).bold()
                    Spacer()
                    #if os(macOS)
                    Button("Kendi penceresinde aç") { AjanPenceresi.ac(motor: motor) }
                        .font(.caption)
                    #endif
                    Button("Kapat", action: kapat)
                }
            }

            // ŞU AN ÇALIŞANLAR — en üstte, çünkü aranan bilgi budur.
            if calisanlar.isEmpty {
                Text(motor.mesgul ? "Görev dağıtılıyor…" : "Şu an çalışan ajan yok.")
                    .font(.caption).foregroundColor(.secondary)
            } else {
                VStack(alignment: .leading, spacing: 6) {
                    Text("Şu an çalışıyor").font(.callout.bold())
                    ForEach(calisanlar) { a in calisan(a) }
                }
                .padding(8)
                .background(Color.accentColor.opacity(0.12)).cornerRadius(6)
            }

            Text("Her soruya alanına göre bir uzman bakar. Birden çok alanı ilgilendiren soruda BİLGE uzmanları bir araya toplar. Aşağıdan, eklediğin yeni belgenin hangi uzmana gideceğini seçebilirsin.")
                .font(.caption).foregroundColor(.secondary)

            ScrollView {
                VStack(alignment: .leading, spacing: 8) {
                    let genel = motor.ajanlar.filter { !$0.uzmanMi }
                    let uzmanlar = motor.ajanlar.filter { $0.uzmanMi }
                    if !genel.isEmpty {
                        Text("Genel görevliler").font(.caption.bold()).foregroundColor(.secondary)
                        ForEach(genel) { a in satir(a) }
                    }
                    if !uzmanlar.isEmpty {
                        Divider()
                        Text("Alan uzmanları").font(.caption.bold()).foregroundColor(.secondary)
                        ForEach(uzmanlar) { a in satir(a) }
                    }
                    if motor.ajanlar.isEmpty {
                        Text("Ajan listesi okunamadı; çekirdek henüz hazır olmayabilir.")
                            .font(.caption).foregroundColor(.secondary)
                    }
                }
            }
        }
        .padding()
        .onAppear { say(); motor.ajanlariYenile(); saatiKur() }
        .onDisappear { saat?.invalidate(); saat = nil }
        .onChange(of: motor.mesgul) { _ in say(); motor.ajanlariYenile() }
    }

    /// Çalışırken ajanlar hızlı değişir; meşgulken sık, boştayken seyrek
    /// yoklanır. Boştayken de yoklanır, çünkü bir ajan arka planda
    /// (animsatıcı yoklaması gibi) iş alabilir.
    private func saatiKur() {
        saat?.invalidate()
        saat = Timer.scheduledTimer(withTimeInterval: 1.0, repeats: true) { _ in
            motor.ajanlariYenile()
        }
    }

    private func calisan(_ a: UmayMotor.Ajan) -> some View {
        HStack(alignment: .top, spacing: 10) {
            AjanResmi.goster(a.alan.isEmpty ? AjanResmi.anahtarla(a.ad) : a.alan, boy: 34)
            VStack(alignment: .leading, spacing: 3) {
                HStack(spacing: 6) {
                    Text(a.ad).bold()
                    Text(a.durumAdi).font(.caption2)
                        .padding(.horizontal, 5).padding(.vertical, 1)
                        .background(Color.accentColor.opacity(0.25)).cornerRadius(4)
                    if a.gecenMs > 0 {
                        Text(sure(a.gecenMs)).font(.caption2).foregroundColor(.secondary)
                    }
                }
                if !a.gorev.isEmpty {
                    Text(a.gorev).font(.caption).lineLimit(3)
                }
                if a.ilerleme > 0 {
                    ProgressView(value: Double(a.ilerleme), total: 100).frame(maxWidth: 220)
                }
            }
            Spacer()
        }
    }

    private func satir(_ a: UmayMotor.Ajan) -> some View {
        let anahtar = a.alan.isEmpty ? AjanResmi.anahtarla(a.ad) : a.alan
        let secili = a.uzmanMi && belgeAlani == a.alan
        return HStack(spacing: 10) {
            AjanResmi.goster(anahtar)
            VStack(alignment: .leading, spacing: 2) {
                HStack(spacing: 6) {
                    Text(a.ad).bold()
                    if a.calisiyorMu {
                        Text(a.durumAdi).font(.caption2)
                            .padding(.horizontal, 5).padding(.vertical, 1)
                            .background(Color.accentColor.opacity(0.25)).cornerRadius(4)
                    }
                }
                Text(a.is_).font(.caption).foregroundColor(.secondary)
                Text(a.turler).font(.caption2).foregroundColor(.secondary)
                if a.uzmanMi {
                    Text("\(sayilar[a.alan] ?? 0) elle yerleştirilmiş belge")
                        .font(.caption2).foregroundColor(.secondary)
                }
            }
            Spacer()
            if a.uzmanMi {
                Button(secili ? "Seçili" : "Belge buna gitsin") {
                    belgeAlani = secili ? "otomatik" : a.alan
                }
                .font(.caption)
            }
        }
        .padding(8)
        .background(secili ? Color.accentColor.opacity(0.18)
                           : (a.calisiyorMu ? Color.accentColor.opacity(0.10)
                                            : Color.gray.opacity(0.12)))
        .cornerRadius(6)
    }

    private func sure(_ ms: UInt32) -> String {
        let sn = Double(ms) / 1000.0
        return sn < 60 ? String(format: "%.1f sn", sn)
                       : String(format: "%d dk %02d sn", Int(sn) / 60, Int(sn) % 60)
    }

    private func say() {
        var s: [String: Int] = [:]
        for b in UmayMotor.belgeleriListele() { s[b.alan.isEmpty ? "otomatik" : b.alan, default: 0] += 1 }
        sayilar = s
    }
}

import SwiftUI

/// Kütüphanedeki belgeler: alanını değiştir ya da unut.
struct BelgeKaydi: Identifiable {
    var id: String { kisa }
    let kisa: String      // gelen/ içindeki dosya adı
    let ozgun: String     // kitabın gerçek adı
    let alan: String      // "" = otomatik
}

extension UmayMotor {
    private static var adlarYolu: URL { gelenDizini.appendingPathComponent("_adlar.txt") }
    private static var islendiYolu: URL { gelenDizini.appendingPathComponent("_islendi.txt") }

    static func belgeleriListele() -> [BelgeKaydi] {
        let fm = FileManager.default
        let dosyalar = ((try? fm.contentsOfDirectory(atPath: gelenDizini.path)) ?? [])
            .filter { !$0.hasPrefix("_") && !$0.hasPrefix(".") }.sorted()
        var eslem: [String: (String, String)] = [:]
        if let t = try? String(contentsOf: adlarYolu, encoding: .utf8) {
            for s in t.split(separator: "\n") {
                let c = s.split(separator: "\t", omittingEmptySubsequences: false).map(String.init)
                if c.count >= 2 { eslem[c[0]] = (c[1], c.count >= 3 ? c[2] : "") }
            }
        }
        return dosyalar.map { BelgeKaydi(kisa: $0, ozgun: eslem[$0]?.0 ?? $0, alan: eslem[$0]?.1 ?? "") }
    }

    /// Belgenin kütüphanedeki bütün parçalarını siler, işlenmişler listesinden çıkarır.
    private static func parcalariSil(_ b: BelgeKaydi) {
        let fm = FileManager.default
        let ozgunBas = String(b.ozgun.prefix(40))
        if let e = fm.enumerator(at: kutDizini, includingPropertiesForKeys: nil) {
            for case let u as URL in e where u.lastPathComponent.hasPrefix("kabul-") && u.pathExtension == "txt" {
                guard let h = try? FileHandle(forReadingFrom: u) else { continue }
                let bas = String(decoding: h.readData(ofLength: 600), as: UTF8.self)
                try? h.close()
                let kay = bas.split(separator: "\n").first(where: { $0.hasPrefix("#kaynak:") }).map(String.init) ?? ""
                if kay.contains(b.kisa) || kay.contains(ozgunBas) { try? fm.removeItem(at: u) }
            }
        }
        if let t = try? String(contentsOf: islendiYolu, encoding: .utf8) {
            let kalan = t.split(separator: "\n", omittingEmptySubsequences: true)
                .filter { !$0.contains(b.kisa) && !$0.contains(ozgunBas) }.joined(separator: "\n")
            try? (kalan.isEmpty ? "" : kalan + "\n").write(to: islendiYolu, atomically: true, encoding: .utf8)
        }
    }

    private static func adSatiriniYaz(_ b: BelgeKaydi, alan: String?) {
        var satirlar: [String] = []
        if let t = try? String(contentsOf: adlarYolu, encoding: .utf8) {
            satirlar = t.split(separator: "\n").map(String.init).filter { !$0.hasPrefix(b.kisa + "\t") }
        }
        if let a = alan {
            if b.kisa != b.ozgun || !a.isEmpty {
                satirlar.append("\(b.kisa)\t\(b.ozgun)" + (a.isEmpty ? "" : "\t\(a)"))
            }
        }
        try? (satirlar.joined(separator: "\n") + (satirlar.isEmpty ? "" : "\n"))
            .write(to: adlarYolu, atomically: true, encoding: .utf8)
    }

    /// Belgeyi başka bir alana taşır: eski parçalar silinir, belge zorunlu alanla yeniden alınır.
    func belgeAlaniniDegistir(_ b: BelgeKaydi, alan: String) {
        guard !mesgul else { return }
        mesgul = true
        sira.async { [weak self] in
            UmayMotor.parcalariSil(b)
            UmayMotor.adSatiriniYaz(b, alan: alan)
            var rapor = [CChar](repeating: 0, count: 8192)
            _ = umay_gomulu_kabul(&rapor, UInt32(rapor.count))
            let ad = alan.isEmpty ? "otomatik" : alan
            let metin = "↪ \(b.ozgun) → alan: \(ad)\n" + String(cString: rapor)
            DispatchQueue.main.async {
                self?.iletiler.append(Ileti(benMi: false, metin: metin, sistemMi: true))
                self?.mesgul = false
            }
        }
    }

    /// Belgeyi kütüphaneden tümüyle kaldırır (dosya da gelen/'den silinir).
    func belgeyiUnut(_ b: BelgeKaydi) {
        guard !mesgul else { return }
        sira.async { [weak self] in
            UmayMotor.parcalariSil(b)
            UmayMotor.adSatiriniYaz(b, alan: nil)
            try? FileManager.default.removeItem(at: UmayMotor.gelenDizini.appendingPathComponent(b.kisa))
            DispatchQueue.main.async {
                self?.iletiler.append(Ileti(benMi: false, metin: "✗ \(b.ozgun) kütüphaneden kaldırıldı.", sistemMi: true))
            }
        }
    }
}

struct BelgeGorunumu: View {
    @ObservedObject var motor: UmayMotor
    @State private var liste: [BelgeKaydi] = []
    @Environment(\.presentationMode) private var kapat

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack { Text("Belgelerim").font(.title2).bold(); Spacer(); Button("Kapat") { kapat.wrappedValue.dismiss() } }
            Text("Yanlış alana giren belgeyi buradan başka alana taşıyabilir ya da kaldırabilirsin.")
                .font(.caption).foregroundColor(.secondary)
            if liste.isEmpty { Text("Henüz belge yok.").foregroundColor(.secondary) }
            ScrollView {
                VStack(alignment: .leading, spacing: 8) {
                    ForEach(liste) { b in satir(b) }
                }
            }
        }
        .padding()
        .onAppear { liste = UmayMotor.belgeleriListele() }
        .onChange(of: motor.mesgul) { _ in liste = UmayMotor.belgeleriListele() }
        #if os(macOS)
        .frame(width: 520, height: 440)
        #endif
    }

    private func satir(_ b: BelgeKaydi) -> some View {
        HStack {
            VStack(alignment: .leading) {
                Text(b.ozgun).lineLimit(2)
                Text("Alan: \(b.alan.isEmpty ? "otomatik" : b.alan)").font(.caption).foregroundColor(.secondary)
            }
            Spacer()
            Menu("Alanı değiştir") {
                ForEach(SohbetGorunumu.belgeAlanlari, id: \.0) { a in
                    Button(a.1) { motor.belgeAlaniniDegistir(b, alan: a.0 == "otomatik" ? "" : a.0) }
                }
            }
            .menuStyle(.borderlessButton).fixedSize().disabled(motor.mesgul)
            Button { motor.belgeyiUnut(b) } label: { Image(systemName: "trash") }
                .disabled(motor.mesgul)
        }
        .padding(6)
        .background(Color.gray.opacity(0.12)).cornerRadius(6)
    }
}

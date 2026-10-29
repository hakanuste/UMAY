import Foundation
import PDFKit
import Vision

/// Taranmış (metni resim olan) PDF'leri Vision ile okur; çıkan yazıyı gelen/'e .txt olarak koyar.
/// Çekirdeğin PDF okuyucusu yalnız gömülü metni görür; resim sayfalar ona boştur.
extension UmayMotor {
    static var taranmisDizini: URL { kutDizini.deletingLastPathComponent().appendingPathComponent("taranmis", isDirectory: true) }

    /// PDF'in metin katmanı sayfa başına çok azsa taranmış sayılır.
    static func taranmisMi(_ belge: PDFDocument) -> Bool {
        let sayfa = belge.pageCount
        guard sayfa > 0 else { return false }
        var yazi = 0
        for i in 0..<min(sayfa, 12) { yazi += belge.page(at: i)?.string?.trimmingCharacters(in: .whitespacesAndNewlines).count ?? 0 }
        return yazi < min(sayfa, 12) * 60
    }

    private static func sayfaGoruntusu(_ p: PDFPage) -> CGImage? {
        let kutu = p.bounds(for: .mediaBox)
        let olcek: CGFloat = max(1, 2000 / max(kutu.width, kutu.height))   // uzun kenar ~2000 px
        let boyut = CGSize(width: kutu.width * olcek, height: kutu.height * olcek)
        let g = p.thumbnail(of: boyut, for: .mediaBox)
        #if os(macOS)
        var r = CGRect(origin: .zero, size: g.size)
        return g.cgImage(forProposedRect: &r, context: nil, hints: nil)
        #else
        return g.cgImage
        #endif
    }

    private static func sayfayiOku(_ cg: CGImage) -> String {
        let istek = VNRecognizeTextRequest()
        istek.recognitionLevel = .accurate
        istek.usesLanguageCorrection = true
        if #available(macOS 13.0, iOS 16.0, *) { istek.recognitionLanguages = ["tr-TR", "en-US"] }
        else { istek.recognitionLanguages = ["en-US"] }
        try? VNImageRequestHandler(cgImage: cg, options: [:]).perform([istek])
        return (istek.results ?? []).compactMap { $0.topCandidates(1).first?.string }.joined(separator: "\n")
    }

    /// gelen/ içindeki taranmış PDF'leri okur. İlerleme `bildir` ile iletilir. Okunanların sayısını döndürür.
    /// Arka kuyrukta çağrılır (uzun sürebilir).
    @discardableResult
    static func taranmisPdfleriOku(bildir: @escaping (String) -> Void) -> Int {
        let fm = FileManager.default
        let dosyalar = ((try? fm.contentsOfDirectory(atPath: gelenDizini.path)) ?? [])
            .filter { !$0.hasPrefix("_") && !$0.hasPrefix(".") && $0.lowercased().hasSuffix(".pdf") }
        var okunan = 0
        for ad in dosyalar {
            let yol = gelenDizini.appendingPathComponent(ad)
            guard let belge = PDFDocument(url: yol), taranmisMi(belge) else { continue }
            let ozgun = belgeleriListele().first(where: { $0.kisa == ad })?.ozgun ?? ad
            let alan = belgeleriListele().first(where: { $0.kisa == ad })?.alan ?? ""
            bildir("⏳ \(ozgun): taranmış sayfalar okunuyor (\(belge.pageCount) sayfa). Uzun sürebilir; uygulamayı açık tut.")
            var metin = ""
            for i in 0..<belge.pageCount {
                autoreleasepool {
                    if let s = belge.page(at: i), let cg = sayfaGoruntusu(s) { metin += sayfayiOku(cg) + "\n\n" }
                }
                if (i + 1) % 100 == 0 { bildir("⏳ \(ozgun): \(i + 1)/\(belge.pageCount) sayfa okundu…") }
            }
            let temiz = metin.trimmingCharacters(in: .whitespacesAndNewlines)
            guard temiz.count > 200 else { bildir("✗ \(ozgun): sayfalardan yazı okunamadı."); continue }
            let govde = ((ad as NSString).deletingPathExtension)
            let txt = kisaAd(govde + "-ocr.txt")
            try? temiz.write(to: gelenDizini.appendingPathComponent(txt), atomically: true, encoding: .utf8)
            adiKaydet(kisa: txt, ozgun: ozgun + " (taranmış)", alan: alan)
            try? fm.createDirectory(at: taranmisDizini, withIntermediateDirectories: true)
            let hedef = taranmisDizini.appendingPathComponent(ad)
            try? fm.removeItem(at: hedef)
            try? fm.moveItem(at: yol, to: hedef)
            okunan += 1
            bildir("✓ \(ozgun): \(temiz.count / 1024) KB yazı okundu.")
        }
        return okunan
    }
}

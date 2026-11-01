import Foundation
import Security
import CryptoKit

/// İsteğe bağlı bulut sesi (ElevenLabs). VARSAYILAN KAPALI: açılırsa yalnızca OKUNACAK yanıt metni ElevenLabs'e gider
/// (kasa, sohbet geçmişi, dosyalar asla gitmez). Anahtar macOS Anahtar Zinciri'nde durur; sohbete, kayda, depoya yazılmaz.
/// Ağ yoksa ya da hata olursa yerel sese (Piper / sistem sesi) dönülür. Aynı metin ikinci kez istenmez (önbellek).
enum BulutSes {
    static let hizmet = "umay.elevenlabs"
    static let varsayilanSes = "J17lijyP1BHYcM7ld0Rg"   // "Adam": derin, ciddi Türkçe erkek sesi (kitaplıktan)

    static func anahtarKaydet(_ deger: String) {
        let sorgu: [String: Any] = [kSecClass as String: kSecClassGenericPassword, kSecAttrService as String: hizmet]
        SecItemDelete(sorgu as CFDictionary)
        onbellek = nil; onbellekOkundu = false
        let t = deger.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !t.isEmpty, let veri = t.data(using: .utf8) else { return }
        var yeni = sorgu; yeni[kSecValueData as String] = veri
        SecItemAdd(yeni as CFDictionary, nil)
    }

    private static var onbellek: String?
    private static var onbellekOkundu = false

    /// Anahtar zincirinde kayıt var mı? (Yalnız öznitelik sorgulanır: parola istemi ÇIKMAZ.)
    static func anahtarVarMi() -> Bool {
        let sorgu: [String: Any] = [kSecClass as String: kSecClassGenericPassword, kSecAttrService as String: hizmet,
                                    kSecReturnAttributes as String: true, kSecMatchLimit as String: kSecMatchLimitOne]
        var sonuc: CFTypeRef?
        return SecItemCopyMatching(sorgu as CFDictionary, &sonuc) == errSecSuccess
    }

    /// Anahtarı okur. İlk okuma macOS'tan parola isteyebilir; bu yüzden ARKA kuyrukta çağrılır ve sonuç bellekte tutulur
    /// (uygulama açık kaldıkça bir daha sorulmaz).
    static func anahtar() -> String? {
        if onbellekOkundu { return onbellek }
        let sorgu: [String: Any] = [kSecClass as String: kSecClassGenericPassword, kSecAttrService as String: hizmet,
                                    kSecReturnData as String: true, kSecMatchLimit as String: kSecMatchLimitOne]
        var sonuc: CFTypeRef?
        let durum = SecItemCopyMatching(sorgu as CFDictionary, &sonuc)
        guard durum == errSecSuccess, let d = sonuc as? Data,
              let s = String(data: d, encoding: .utf8), !s.isEmpty else {
            // Kullanıcı "Reddet" dediyse bu oturumda yeniden sorma (yerel sese dönülür).
            if durum == errSecAuthFailed || durum == errSecUserCanceled || durum == errSecInteractionNotAllowed { onbellekOkundu = true }
            return nil
        }
        onbellek = s; onbellekOkundu = true
        return s
    }

    /// Kullanıcı bulut sesini açtı mı? (Ucuz: anahtar zincirine dokunmaz, ana kuyrukta güvenle çağrılır.)
    static var istekli: Bool { UserDefaults.standard.bool(forKey: "umay.bulutSes") && anahtarVarMi() }

    private static var onbellekDizini: URL {
        let d = FileManager.default.urls(for: .cachesDirectory, in: .userDomainMask)[0].appendingPathComponent("umay-bulut-ses", isDirectory: true)
        try? FileManager.default.createDirectory(at: d, withIntermediateDirectories: true)
        return d
    }

    /// Bekleyişi kısaltmak için: ilk cümle(ler) hemen istenip çalınır, kalanı arkada hazırlanır.
    static func bol(_ metin: String) -> (String, String) {
        let t = metin.trimmingCharacters(in: .whitespacesAndNewlines)
        guard t.count > 120 else { return (t, "") }
        let bitis: Set<Character> = [".", "!", "?", "…", "\n"]
        var ilk = ""
        for (i, c) in t.enumerated() {
            ilk.append(c)
            if i >= 40 && bitis.contains(c) { break }
        }
        let kalan = String(t.dropFirst(ilk.count)).trimmingCharacters(in: .whitespacesAndNewlines)
        return (ilk.trimmingCharacters(in: .whitespacesAndNewlines), kalan)
    }

    /// Metni mp3'e çevirir (ağ çağrısı: arka kuyrukta çağır). Başarılıysa dosya adresini döner.
    static func uret(_ metin: String) -> URL? {
        guard let anahtar = anahtar() else { return nil }
        let ses = (UserDefaults.standard.string(forKey: "umay.bulutSesNo") ?? "").trimmingCharacters(in: .whitespaces)
        let sesNo = ses.isEmpty ? varsayilanSes : ses
        let hiz = max(0.7, min(1.2, UserDefaults.standard.object(forKey: "umay.bulutHiz") as? Double ?? 1.0))
        let ozet = SHA256.hash(data: Data((sesNo + "|" + String(format: "%.2f", hiz) + "|" + metin).utf8)).map { String(format: "%02x", $0) }.joined().prefix(24)
        let dosya = onbellekDizini.appendingPathComponent("\(ozet).mp3")
        if FileManager.default.fileExists(atPath: dosya.path) { return dosya }
        guard let adres = URL(string: "https://api.elevenlabs.io/v1/text-to-speech/\(sesNo)?output_format=mp3_44100_128") else { return nil }
        var istek = URLRequest(url: adres, timeoutInterval: 20)
        istek.httpMethod = "POST"
        istek.setValue(anahtar, forHTTPHeaderField: "xi-api-key")
        istek.setValue("application/json", forHTTPHeaderField: "Content-Type")
        istek.setValue("audio/mpeg", forHTTPHeaderField: "Accept")
        let govde: [String: Any] = ["text": metin, "model_id": "eleven_flash_v2_5", "language_code": "tr",
                                     "voice_settings": ["speed": hiz, "stability": 0.66, "similarity_boost": 0.75]]
        istek.httpBody = try? JSONSerialization.data(withJSONObject: govde)
        var sonuc: URL? = nil
        let bekle = DispatchSemaphore(value: 0)
        URLSession.shared.dataTask(with: istek) { veri, yanit, _ in
            defer { bekle.signal() }
            guard let h = yanit as? HTTPURLResponse, h.statusCode == 200, let veri = veri, veri.count > 1000 else { return }
            if (try? veri.write(to: dosya)) != nil { sonuc = dosya }
        }.resume()
        _ = bekle.wait(timeout: .now() + 25)
        return sonuc
    }
}

/// Arka kuyruktan ana kuyruğa sonuç taşımak için basit kutu.
final class SonucKutusu { var adres: URL? }

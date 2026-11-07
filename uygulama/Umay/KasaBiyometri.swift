import Foundation
import Security
import LocalAuthentication

/// KASA — BİYOMETRİK AÇMA (yüz / parmak izi).
///
/// NEDEN AYRI DOSYA: çekirdek tarafı (core/umay_kasa.c) ve köprü
/// işlevleri (umay_gomulu_kasa_anahtar, umay_gomulu_kasa_ac_anahtarla)
/// yazılmış ve sınanmıştı, ama uygulama hiçbirini ÇAĞIRMIYORDU —
/// yani yüz ile açma yalnız kâğıt üzerinde vardı. Bu dosya o boşluğu
/// kapatır ve işletim sistemine bakan bütün işi bir yerde tutar.
///
/// NASIL ÇALIŞIR
///   1. Kullanıcı kasayı ANA PAROLAYLA açar.
///   2. İsterse "yüz ile aç"ı açar: çekirdekten 32 baytlık kasa
///      anahtarı alınır ve işletim sisteminin biyometri korumalı
///      deposuna yazılır. Anahtar bir daha oradan çıkmaz.
///   3. Sonraki açılışlarda anahtar okunur — okuma biyometri ister —
///      ve kasa `kasa_ac_anahtarla` ile açılır. Parola girilmez.
///
/// GÜVENLİK SEÇİMLERİ, GEREKÇELERİYLE
///   - `.biometryCurrentSet`: aygıta YENİ bir yüz/parmak eklenirse
///     kayıt KENDİLİĞİNDEN geçersiz olur. `.biometryAny` olsaydı,
///     aygıta kendi yüzünü ekleyen biri kasayı açabilirdi.
///   - Parola yedeği YOK: `.or devicePasscode` bilinçli olarak
///     kullanılmadı. Olsaydı aygıt parolasını bilen biri biyometriyi
///     atlardı ve bu kapı ana paroladan ZAYIF olurdu.
///   - `WhenUnlockedThisDeviceOnly`: iCloud'a eşitlenmez, yedeğe
///     girmez, başka aygıta geçmez. Anahtar bu aygıtta kalır.
///   - Anahtar baytları kullanıldıktan hemen sonra sıfırlanır.
///
/// CANLILIK (liveness) ÜZERİNE DÜRÜST NOT
/// Face ID'nin kendi derinlik ölçümü ve dikkat denetimi vardır;
/// fotoğrafla açılmaz. Bunu uygulama katmanından ZORLAYAMAM ve
/// kendi canlılık sınamamı ekleyemem — yapılabilecek olan, yedek
/// yolları kapatıp (parola yedeği yok) biyometri kümesi değişince
/// kaydı geçersiz saymaktır. İkisi de yapıldı. Bundan fazlasını
/// "canlılık doğrulandı" diye sunmak yanlış olur.
enum KasaBiyometri {
    static let hizmet = "umay.kasa.anahtar"
    static let anahtarBoyu = 32

    /// Aygıtta biyometri var, kurulu ve kullanılabilir mi.
    static var elverisli: Bool {
        var hata: NSError?
        let baglam = LAContext()
        return baglam.canEvaluatePolicy(.deviceOwnerAuthenticationWithBiometrics, error: &hata)
    }

    /// Biyometrinin adı (arayüzde doğru sözcüğü yazmak için).
    static var ad: String {
        let baglam = LAContext()
        _ = baglam.canEvaluatePolicy(.deviceOwnerAuthenticationWithBiometrics, error: nil)
        switch baglam.biometryType {
        case .faceID:  return "Face ID"
        case .touchID: return "Touch ID"
        default:       return "biyometri"
        }
    }

    /// Kayıt var mı? YALNIZ ÖZNİTELİK sorgulanır, veri istenmez:
    /// bu yüzden biyometri istemi ÇIKMAZ. (Aynı kalıp Bulut.swift'te
    /// anahtar zinciri yoklaması için de kullanılıyor.)
    static var kayitli: Bool {
        let sorgu: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: hizmet,
            kSecReturnAttributes as String: true,
            kSecMatchLimit as String: kSecMatchLimitOne]
        var sonuc: CFTypeRef?
        return SecItemCopyMatching(sorgu as CFDictionary, &sonuc) == errSecSuccess
    }

    /// Kaydı siler. Biyometri istemez.
    static func sil() {
        let sorgu: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: hizmet]
        SecItemDelete(sorgu as CFDictionary)
    }

    /// KASA AÇIKKEN çağrılır: anahtarı çekirdekten alıp biyometri
    /// korumalı depoya yazar. 0 = oldu.
    ///
    /// Dönen kodlar: -6 kasa kilitli (çekirdekten), -20 biyometri yok,
    /// -21 erişim kuralı kurulamadı, -22 depoya yazılamadı.
    static func kaydet() -> Int32 {
        guard elverisli else { return -20 }

        var anahtar = [UInt8](repeating: 0, count: anahtarBoyu)
        let kod = anahtar.withUnsafeMutableBufferPointer { p -> Int32 in
            guard let t = p.baseAddress else { return -22 }
            return umay_gomulu_kasa_anahtar(t)
        }
        // Çekirdek anahtarı yalnız kasa AÇIKKEN verir; kilitliyse -6.
        if kod != 0 { sifirla(&anahtar); return kod }

        var hata: Unmanaged<CFError>?
        let kural = SecAccessControlCreateWithFlags(
            nil,
            kSecAttrAccessibleWhenUnlockedThisDeviceOnly,   // eşitlenmez, yedeğe girmez
            .biometryCurrentSet,                            // yeni yüz eklenirse geçersiz
            &hata)
        guard let erisim = kural else { sifirla(&anahtar); return -21 }

        sil()   // eskisi varsa üstüne yazmak yerine önce temizle

        let veri = Data(anahtar)
        sifirla(&anahtar)
        let yeni: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: hizmet,
            kSecAttrAccessControl as String: erisim,
            kSecValueData as String: veri]
        let d = SecItemAdd(yeni as CFDictionary, nil)
        return d == errSecSuccess ? 0 : -22
    }

    /// Biyometri ile kasayı açar. Okuma biyometri istemini çıkarır.
    /// ARKA KUYRUKTA çağır: istem ana kuyruğu kilitlemesin.
    ///
    /// Dönen kodlar: 0 oldu, -2 anahtar yanlış (çekirdekten),
    /// -20 biyometri yok, -23 kayıt yok ya da okunamadı
    /// (kullanıcı vazgeçti, biyometri değişti, kilit açılmadı).
    static func ac() -> Int32 {
        guard elverisli else { return -20 }

        let baglam = LAContext()
        baglam.localizedReason = "Kasayı açmak için"
        let sorgu: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: hizmet,
            kSecReturnData as String: true,
            kSecMatchLimit as String: kSecMatchLimitOne,
            kSecUseAuthenticationContext as String: baglam]
        var sonuc: CFTypeRef?
        let d = SecItemCopyMatching(sorgu as CFDictionary, &sonuc)
        guard d == errSecSuccess, let veri = sonuc as? Data, veri.count == anahtarBoyu else {
            return -23
        }

        var anahtar = [UInt8](repeating: 0, count: anahtarBoyu)
        veri.copyBytes(to: &anahtar, count: anahtarBoyu)
        let kod = anahtar.withUnsafeBufferPointer { p -> Int32 in
            guard let t = p.baseAddress else { return -23 }
            return umay_gomulu_kasa_ac_anahtarla(t)
        }
        sifirla(&anahtar)
        return kod
    }

    /// Anahtar baytlarını bellekte sıfırlar. Derleyici bu döngüyü
    /// atmasın diye dizi yerinde yazılır ve sonucu okunur.
    private static func sifirla(_ a: inout [UInt8]) {
        for i in 0..<a.count { a[i] = 0 }
        _ = a.reduce(0) { $0 &+ Int($1) }
    }

    /// Arayüzde gösterilecek karşılık.
    static func hataMetni(_ kod: Int32) -> String {
        switch kod {
        case -20: return "Bu aygıtta kullanılabilir biyometri yok."
        case -21: return "Biyometri koruması kurulamadı."
        case -22: return "Anahtar aygıt deposuna yazılamadı."
        case -23: return "\(ad) ile açılamadı. Kayıt yok, biyometri değişmiş ya da vazgeçildi; ana parolayla aç."
        default:  return KasaDurumu.hataMetni(kod)
        }
    }
}

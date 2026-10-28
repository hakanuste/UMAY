# UMAY uygulaması (macOS + iOS) — SwiftUI

Tek kod, iki platform. Çekirdek C'dir (`umay_gomulu.h` ile bağlanır); Swift yalnız yüzü çizer.
**DURUM: Swift kodu bu ortamda DERLENMEDİ** (Linux'ta Swift derleyicisi yok). C tarafı (motorlu kitaplık + gömülü arayüz) Linux'ta sınandı.
İlk derleme sizin elinizden çıkacak; derleme hatası olursa çıktıyı gönderin.

## Ne var
- Sohbet ekranı; üstte **dürüst durum**: gerçek dil modeli yüklüyse adı, yoksa "Yedek motor — dil modeli yok".
- Yanıtı Türkçe sesle okuma (sistem sesi, Piper gerekmez).
- Belge ekle (.txt .md .pdf): `gelen/` klasörüne kopyalanır, ajanlar inceleyip yerleştirir (KABUL), rapor sohbette görünür.
- Ayarlar: dil modeli indirme (Hugging Face; iOS'ta yalnız 0,6B ve 1,7B, 4B yalnız Mac'te), "internetten öğren" anahtarı (varsayılan KAPALI).
- Veri, uygulamanın yazabildiği yerde durur (`Application Support/Umay/kut`); uygulama paketindeki `kut/` ilk açılışta kopyalanır, kullanıcı dosyalarının üstüne yazılmaz.

## macOS — tam Xcode GEREKMEZ (2017 Air'de de olur)
```
./arac/mac_uygulama.sh --model       # kov/Umay.app üretir, orta modeli indirir
open kov/Umay.app
```
Gerekli: Xcode komut satırı araçları (`swiftc`), `cmake`. İlk motor derlemesi birkaç dakika.
`Umay.app` yerel imzalıdır; ilk açışta "tanımlanamayan geliştirici" uyarısı çıkarsa: sağ tık → Aç.

## iOS (iPhone) — tam Xcode GEREKİR, yeni bir Mac ister
```
./arac/uygulama_kitaplik.sh xcframework   # kov/uyg/Umay.xcframework
brew install xcodegen && cd uygulama && xcodegen generate && open Umay.xcodeproj
```
Xcode'da takımınızı (ücretsiz Apple kimliği yeter, uygulama 7 gün çalışır) seçip iPhone'a yükleyin. iPhone'da 1,7B model (1,8 GB) önerilir; iOS bir uygulamaya kabaca 3 GB bellek verir, 4B bu sınıra yaklaştığı için sunulmaz.
Çekirdekte iOS'ta yasak olan alt süreç çağrıları (`popen`, `system`) `port/umay_host_uyum.c` içinde kapatıldı; ağı uygulama getirir.

## Bilinen sınırlar
- Yalnız işlemci (CPU) çıkarımı; Metal/GPU hızlandırma yok. 2017 Air'de 1,7B model bir yanıt için yaklaşık bir dakika sürdü.
- Model değişikliği ve "öğren" ayarı uygulama yeniden açılınca işler.
- Sohbet geçmişi saklanmaz (BENGİ belleği çekirdekte tutulur).

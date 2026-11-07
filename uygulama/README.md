# UMAY uygulaması (macOS + iOS) — SwiftUI

Tek kod, iki platform. Çekirdek C'dir (`umay_gomulu.h` ile bağlanır); Swift yalnız yüzü çizer.
**DURUM:** Swift kodu GitHub Actions macOS makinesinde **derlendi**: macOS uygulaması (`swiftc`, gömülü motorla) ve iOS Benzetici (`xcodebuild`). Gerçek Mac'te ve iPhone'da **çalıştırılmadı**; arayüzün davranışı henüz görülmedi. C tarafı (motorlu kitaplık + gömülü arayüz) Linux'ta sınandı.
İlk derleme sizin elinizden çıkacak; derleme hatası olursa çıktıyı gönderin.

## Ne var
- Sohbet ekranı; üstte **dürüst durum**: gerçek dil modeli yüklüyse adı, yoksa "Yedek motor — dil modeli yok".
- Yanıtı Türkçe sesle okuma. Sıra: (isteğe bağlı) bulut sesi → Piper (yerel, sinir ağı) → macOS sistem sesi. Ayarlar'da hız ve "cızırtı payı" kaydırıcıları.
- **Piper (çevrimdışı ses):** `./ses/piper_kur.sh` ya da elle; modeller `~/.local/share/umay/ses/` içinde `*.onnx` + `*.onnx.json`; Ayarlar → Ses modeli.
- **Bulut sesi (ElevenLabs), isteğe bağlı, varsayılan KAPALI:** açılırsa yalnız okunacak yanıt metni gider. API anahtarı (`sk_…`) Ayarlar'daki kutuya yazılır ve Anahtar Zinciri'nde saklanır; depoya ve sohbete yazılmaz. Kitaplık sesleri ücretli plan ister (HTTP 402); kendi tasarladığın ses (Voice Design) ya da hazır sesler ücretsiz planda çalışır. Hata/ağ yoksa yerel sese dönülür.
- **Tek pencerede çok bölme:** sol şerit (Sohbet, Ajanlar, Belgelerim, Kasa) + sohbet + sağ pano. Ajanlar panosu: BİLGE, KABUL, ARAŞTIRMACI, ÇEVİRMEN, GÖZCÜ, ONARICI, YAZMAN ve alan uzmanları; amblemler `arac/amblem_kes.py` ile `docs/gorsel/`dan üretilir.
- **Gecikme ölçümü:** başlıkta `yanıt X sn · ses +Y sn (tür)`.
- **Eğitim verisi (yerel):** "doğru, ekle" dediğin çiftler `Application Support/Umay/egitim/onayli.jsonl` dosyasında birikir; `python3 arac/egitim_verisi.py` onları gizlilik süzgecinden geçirip ince ayar (LoRA) biçimine çevirir. Dosyalar depoya girmez.
- Belge ekle (.txt .md .pdf): `gelen/` klasörüne kopyalanır, ajanlar inceleyip yerleştirir (KABUL), rapor sohbette görünür.
- Ayarlar: dil modeli indirme (Hugging Face; iOS'ta yalnız 0,6B ve 1,7B, 4B yalnız Mac'te), "Kayıt yoksa internetten ara ve öğren" anahtarı (varsayılan AÇIK; kapatınca uygulama tümüyle çevrimdışı çalışır). Bulunan bilgi "(doğrulanmadı)" notuyla gelir; **"doğru, ekle"** dersen kütüphaneye yazılır, **"yanlış"** dersen silinir.
- Veri, uygulamanın yazabildiği yerde durur (`Application Support/Umay/kut`); uygulama paketindeki `kut/` ilk açılışta kopyalanır, kullanıcı dosyalarının üstüne yazılmaz.

## macOS — tam Xcode GEREKMEZ (2017 Air'de de olur)
```
./arac/mac_uygulama.sh --model       # kov/Umay.app üretir, orta modeli indirir
open kov/Umay.app
```
Gerekli: Xcode komut satırı araçları (`swiftc`), `cmake`. İlk motor derlemesi birkaç dakika.
`Umay.app` yerel imzalıdır; ilk açışta "tanımlanamayan geliştirici" uyarısı çıkarsa: sağ tık → Aç.

**Kurulum (Launchpad/Applications, simgesiyle):** `./arac/mac_kur.sh --derle` → `/Applications/Umay.app` (Dock'ta tut: simgeye sağ tık). `--derle` olmadan önceki derlemeyi kurar. Güncelleme: `git pull`, `rm -rf kov/uyg/macos` (işlemci özelliği değiştiyse), `./arac/mac_kur.sh --derle`.
**Hız:** Intel Mac'te işlemci AVX2/FMA/F16C veriyorsa derleme bunları açar; yanıt en çok üç cümle/200 jeton. Daha hızlısı için Ayarlar'dan küçük (0,6B) modeli seç. Boşta çalışan "eğit" döngüsü son iletiden sonra 3 dakika susar.

## iOS (iPhone) — tam Xcode GEREKİR, yeni bir Mac ister
```
./arac/uygulama_kitaplik.sh xcframework   # kov/uyg/Umay.xcframework
brew install xcodegen && cd uygulama && xcodegen generate && open Umay.xcodeproj
```
Uygulama simgesi `Umay/Assets.xcassets/AppIcon` içinde hazırdır (`python3 arac/ikon_uret.py` yeniden üretir; iOS için saydamsız, tam kapsayan turkuaz zemin).
Xcode'da takımınızı (ücretsiz Apple kimliği yeter, uygulama 7 gün çalışır) seçip iPhone'a yükleyin. iPhone'da 1,7B model (1,8 GB) önerilir; iOS bir uygulamaya kabaca 3 GB bellek verir, 4B bu sınıra yaklaştığı için sunulmaz.
Çekirdekte iOS'ta yasak olan alt süreç çağrıları (`popen`, `system`) `port/umay_host_uyum.c` içinde kapatıldı; ağı uygulama getirir.

## Bilinen sınırlar
- Yalnız işlemci (CPU) çıkarımı; Metal/GPU hızlandırma yok. 2015–2017 Air'de 1,7B model bir yanıt için yarım dakika–bir dakika sürebilir; hızlı çekirdekler ve küçük model bunu kısaltır.
- Bulut sesi (ElevenLabs) ve internet araması isteğe bağlıdır; ikisi kapalıyken uygulama tümüyle çevrimdışı çalışır.
- Ses klonlama/ince ayar çalışması (kendi sesiyle Piper eğitimi) denendi ve sonuç (tiz, cızırtılı) beklenen kaliteye ulaşmadığı için depodan çıkarıldı.
- Model değişikliği ve "öğren" ayarı uygulama yeniden açılınca işler.
- Sohbet geçmişi yalnız bu aygıtta saklanır (son 200 ileti, `Application Support/Umay/sohbet.json`); sağ üstteki çöp kutusu siler.
- Sesle soru sorma (mikrofon) henüz yok.

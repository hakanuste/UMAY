# UMAY gömülü dil modeli (motor)

UMAY'ın çekirdeği C11'dir ve değişmez. Dil modeli, çekirdeğin dışında, **C ABI** ile (`include/umay_motor.h`) bağlanır:

- `us/umay_motor_yok.c` → motor yok (`umay_motor_api()` = NULL). Varsayılan yapı budur.
- `motor/umay_motor.cpp` → llama.cpp'yi sarar (`motor/llama.cpp/`).

Hangisinin bağlanacağı **bağlama anında** seçilir (`arac/motor.sh`); çekirdekte `#ifdef` yoktur.

## Sıra ve dürüstlük
`US` motor sırası: **llama (gömülü)** → ollama → US1 → stub. `UMAY_LLM=0` gömülüyü kapatır.
- Motor ya da model yoksa "hazır" denmez; açık neden günlüğe yazılır, bir alttaki motora düşülür.
- Çıktı geçerli UTF-8 ve okunur değilse (`us_llama_cikti_saglam`) yanıt sayılmaz.
- Bağlam taşarsa bir kez yeniden denenir. Yanıt yalnız DAYANAK'a dayanır; yetmiyorsa "bulamadım" der.

## Kurulum (Linux/macOS, ana bilgisayar)
```
./arac/motor.sh                 # motorlu yapı: kov/umay-motorlu (ilk derleme ~2 dk)
./kur.sh --model                # belleğe göre model indirir
./kur.sh --model=orta           # kucuk | orta | buyuk
UMAY_LLM=/yol/model.gguf umay "soru"   # elle model
```
Modeller (Hugging Face'te dosya adları doğrulandı):

| seçim | dosya | boyut |
|---|---|---|
| kucuk | Qwen/Qwen3-0.6B-GGUF · Qwen3-0.6B-Q8_0.gguf | ~0,6 GB |
| orta | Qwen/Qwen3-1.7B-GGUF · Qwen3-1.7B-Q8_0.gguf | ~1,8 GB |
| buyuk | Qwen/Qwen3-4B-GGUF · Qwen3-4B-Q4_K_M.gguf | ~2,5 GB |

Model `<kut>/model/` altına konur. Ayarlar (`ayar.txt`): `llm` (dosya yolu), `llm_baglam` (varsayılan 2048), `llm_is` (iş parçacığı; 0 = otomatik).

## Diğer platformlar (belgelenmiş, henüz denenmedi)
- **Windows**: llama.cpp'yi MSVC/zig ile derleyip `motor/umay_motor.cpp` ile bağlayın.
- **Android**: NDK + CMake ile `motor/CMakeLists.txt`; model uygulama dizinine kopyalanır.
- **iOS**: dlopen yok → statik kitaplık olarak bağlanır; Metal isteğe bağlı. Modeller uygulama paketine/belgeler dizinine konur.

## Doğrulananlar / doğrulanmayanlar
- Doğrulandı: motor Linux x64'te derlenir; rastgele ağırlıklı küçük sınama modeliyle (`arac/mini_gguf.py`) yükleme → belirteçleme → çözme → örnekleme → koruma → stub düşüşü uçtan uca çalışır.
- **Doğrulanmadı**: gerçek model kalitesi ve hızı (bu ortamda ağırlık indirilemedi), macOS/iOS/Android/Windows motor derlemeleri.

## Sonraki adım
Seçenek 2: hazır bir modelin Hakan'ın belgeleriyle ince ayarı (Hugging Face hesabı: Hakanuste).

## macOS'ta hızlı sınama
```
xcode-select --install ; brew install cmake
./arac/mac_sina.sh --model     # derler, sınar, motoru kurar, küçük modeli indirir; rapor: mac_rapor.txt
```
macOS'ta motor ilk yapımda Metal/Accelerate/OpenMP kapalı (en güvenli); hız için sonra açılır.

## Öz Türkçe süzgeci
Gömülü model yanıt verince `core/umay_ozturk.c` yabancı kökenli sözcük arar (ek/çekim alır: tefekkürün). Bulursa model bir kez "şu sözcükleri kullanmadan yeniden yaz" diye uyarılır; yeni yazım bozuksa ya da daha az sözcük düzeltmişse özgün yanıt korunur ve günlüğe yazılır — yanıt sessizce değiştirilmez.
Gömülü liste bir ÖNERİdir. Kendi sözcüklerinizi `<kut>/ozturkce.txt` dosyasına `yabancı=karşılık` satırlarıyla ekleyin (`#` yorum). Divanü Lügati't-Türk kaynaklı karşılıklar için asıl metin kaynak alınmalıdır.

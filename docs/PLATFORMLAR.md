# UMAY — platformlar

UMAY tek bir C kod tabanıdır. Platform farkları **yalnız** `port/` ve
`include/umay_host.h` arkasındadır; çekirdekte (`core/`, `alp/`, `us/`)
platforma göre koşullu derleme yoktur.

## Durum (neyin kanıtlandığı)

| Platform | Nasıl kurulur | Kanıt |
|---|---|---|
| Linux x64 | `./kur.sh` ya da `umay-*-linux-x64.tar.gz` | 268 sınama + gömülü 11, yerel koşuldu; ASan/UBSan temiz |
| Linux ARM64, **Android (Termux)** | `umay-*-linux-arm64.tar.gz` | 268 + 11, `qemu-aarch64` altında koşuldu |
| Windows x64 | `umay-*-windows-x64.zip`, aç, `umay.exe` | 268 + 11, **Wine** altında koşuldu; Ollama TCP yolu sahte sunucuyla sınandı |
| Windows ARM64 | `umay-*-windows-arm64.zip` | yalnız **derlendi** (çalıştırılamadı) |
| macOS (Intel, Apple Silicon) | `./kur.sh` ya da `umay-*-macos-*.tar.gz` | yalnız **derlendi** (bu ortamda Mac yok) |
| Android uygulaması (JNI) | `CMakeLists.txt` `-DUMAY_JNI=ON` | JNI köprüsü JVM'de uçtan uca sınandı; **Android'de denenmedi** |
| iOS uygulaması | `libumay.a` + `umay_gomulu.h` | **derlenmedi, denenmedi** (Xcode/Swift yok) |

"Derlendi" yalnız derleyicinin kabul ettiği demektir; çalıştığı demek **değildir**.
Gerçek Windows, Mac ve telefonda ilk koşu sizin elinizden çıkacak.

## Taşınabilir paket

Paket açılınca `umay` (ya da `umay.exe`) ve yanında `kut/` bulunur. Program
verisini kendi yanında arar (`<program dizini>/kut`), hangi dizinden
çalıştırılırsa çalıştırılsın. Arama sırası: `UMAY_KUT`, `./kut`,
`~/.local/share/umay/kut` (Windows'ta `%USERPROFILE%\.local\share\umay\kut`),
program dizini, `/usr/local/share/umay/kut`.

    Windows:   umay.exe "güneş paneli nedir"
    Linux/mac: ./umay "güneş paneli nedir"

## Ses

- Masaüstünde Dede Korkut sesi Piper ile üretilir (`./kur.sh --ses`; Windows'ta
  `piper.exe` ya da `python` + `onnxruntime` + `espeak-ng` PATH'te olmalı).
  Windows'ta çalıcı olarak PowerShell `Media.SoundPlayer` kullanılır
  (gerçek Windows'ta denenmedi).
- Termux'ta `termux-media-player` (termux-api) çalıcı olarak denenir.
- Telefon uygulamasında **Piper gerekmez**: gömülü kitaplık yanıtı metin olarak
  verir, uygulama sistemin Türkçe sesini kullanır (Android `TextToSpeech`,
  iOS `AVSpeechSynthesizer`).

## Uygulamaya gömmek

`include/umay_gomulu.h` üç işlev verir: `umay_gomulu_baslat`, `umay_gomulu_sor`,
`umay_gomulu_kapat`. Veri dizini **yazılabilir** olmalıdır (BENGİ hafızası oraya
saklanır). Tek iplikten çağırın.

### Android (Android Studio)

`app/build.gradle` içinde:

    externalNativeBuild { cmake { path "<UMAY>/CMakeLists.txt"
                                  arguments "-DUMAY_JNI=ON" } }

`port/android/tr/umay/Umay.java` dosyasını projeye kopyalayın:

    Umay.baslat(getFilesDir().getAbsolutePath() + "/kut");   // kut/ önce kopyalanmalı
    String yanit = Umay.sor("güneş paneli nedir");

`kut/` klasörü APK içinde salt okunurdur; ilk açılışta `getFilesDir()` altına
kopyalayın.

### iOS (Xcode)

`CMakeLists.txt` ile (ya da Xcode'a kaynakları ekleyerek) statik kitaplık üretin,
`umay_gomulu.h`'yi bridging header'a ekleyin:

    umay_gomulu_baslat(veriYolu)
    var tampon = [CChar](repeating: 0, count: 8192)
    let n = umay_gomulu_sor("güneş paneli nedir", &tampon, UInt32(tampon.count))
    let yanit = String(cString: tampon)

Bu kısım **bu ortamda derlenemedi**. iOS'ta komut satırı programı çalışmaz;
kuruluma Apple geliştirici hesabı (TestFlight ya da cihaza doğrudan yükleme)
gerekir.

## Derleme ve sınama

    arac/capraz.sh              # tüm hedefleri derler, çalıştırabildiğini sınar
    arac/capraz.sh --paketle    # kov/paket/ altına sürüm paketlerini üretir

Gerekli: `pip install ziglang`. Sınama için Windows'ta `wine`, ARM64'te
`qemu-aarch64` aranır; bulunmayan hedef yalnız derlenir ve tabloda
"calistirilamadi" yazar.

## Bilinen sınırlar

- Windows'ta alt süreç komutları `cmd.exe` üzerinden gider; çok uzun (16 KB üstü)
  komutlar reddedilir (kırpılıp yanlış çalışmasın diye).
- Windows konsolunda UTF-8 ve renk kaçışları açılır (Windows 10 1809+).
- Android'de `/tmp` yoktur; `TMPDIR` kullanılır.
- JNI "değiştirilmiş UTF-8" kullanır; Türkçe harfler (BMP) için ayrım yoktur,
  BMP dışı karakterlerde (emoji) fark olur.

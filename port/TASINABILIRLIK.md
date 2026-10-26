# UMAY'ı her işletim sistemine taşımak

**UMAY bir Linux programı değildir.** Hedef: Android, iOS, Windows,
macOS — ve ÖTÜKEN.

Bu belge ölçümle yazıldı. Doğrulanan ile doğrulanamayan ayrı duruyor;
"derlenir" ile "koşar" aynı şey değil ve bu depoda bir kez pahalıya
öğrenildi (`umay.ld` `ENTRY(_start)` diyordu, `_start` yoktu, ELF
**bağlanıyordu** ve giriş `um_bellek_kopya`ya düşüyordu).

## Mimari neden hazır: ölçülmüş kanıt

Çekirdekte (`core/`, `alp/`, `us/`) **tek bir işletim sistemi başlığı
yok**. Geçen tek şey `stdarg.h`, o da serbest duran C'nin parçası.

Kanıt soyut değil, `make otuken-denetle` her koşumda basıyor:

    sozdizimi: 22 dosya derlendi.
    baglama: _start tanimli, cozulmemis simge yok (22 nesne).

22 dosya **serbest duran** kipte (`-ffreestanding`, libc yok) derlenip
nesneye iniyor ve **sıfır çözülmemiş konak simgesi** veriyor. Yani
çekirdek şu anda bile hiçbir işletim sistemine bağlı değil.

Bir platform eklemek üç parçadır:

1. **Port dosyası** — `include/umay_port.h`'deki **35 işlevi** gerçekler
2. **Birim listesi** — o platformda var olamayan konak birimleri
   Makefile'da `filter-out` ile çıkarılır (ÖTÜKEN bunu zaten yapıyor)
3. **Ön yüz** — konsol, ÜLGEN, ya da platformun kendi arayüzü

Çekirdeğe dokunulmaz. Bugün iki port var: `umay_port_host.c` (POSIX) ve
`umay_port_otuken.c` (çağrı numaralarıyla).

## Konak birimlerinin platform engelleri (ölçüldü)

| Birim | Kullandığı | Nerede olamaz |
|---|---|---|
| `ses/umay_piper.c` | `popen`, `system` | iOS (alt süreç yasak), ÖTÜKEN |
| `gui/umay_gui_konsol.c` | `isatty`, `fcntl` | iOS/Android app (uçbirim yok) |
| `us/umay_us_ollama.c` | `socket`, `connect` | ÖTÜKEN (TCP yığını yok) |

Hiçbiri **zorunlu değil**: üçü de yokken UMAY alıntı çıkaran motorla
yanıt verir. Bu, ÖTÜKEN yapımında zaten böyle.

## Platform platform

### macOS — POSIX portu yeter
`make posix` **katı POSIX** kipinde (`_POSIX_C_SOURCE=200809L`,
`-Werror`) temiz derleniyor; glibc'ye özgü uzantı yok. Bu kapı
gerçekten iş görüyor — sınandı, `strfry` sokulunca yakaladı.

Bu bir kez gerçek bir hatayı önledi: `up_uyu` `usleep()` çağırıyordu.
`usleep` POSIX.1-2008'de **kaldırıldı**; glibc onu `_DEFAULT_SOURCE`
ile geri veriyor ama macOS'un karşılığı `_DARWIN_C_SOURCE`. Linux'ta
sessizce derlenen kod macOS'ta kırılacaktı. Artık `nanosleep`.

**Doğrulanan:** katı POSIX derlemesi. **Doğrulanmayan:** gerçek macOS
üzerinde koşum — bu kapta macOS yok.

### Windows — iki yol
- **MSYS2 / Cygwin:** bugünkü POSIX portu olduğu gibi derlenir.
  `kur.sh` artık `MSYS*`, `MINGW*`, `CYGWIN*` tanıyor ve doğru
  derleyici önerisini veriyor.
- **Yerli Win32:** yeni bir port dosyası, 35 işlev. Eşlemeler mekanik:
  `open/read/write/close` → `CreateFile/ReadFile/WriteFile/CloseHandle`,
  `opendir/readdir` → `FindFirstFile/FindNextFile`,
  `clock_gettime` → `QueryPerformanceCounter`, `nanosleep` → `Sleep`,
  BSD yuvaları → Winsock2 (`WSAStartup` gerekir),
  `popen/system` → `CreateProcess`.

**Doğrulanmayan ve nedeni:** bu kapta MinGW yok ve `apt` paketi bulamıyor
(`E: Unable to locate package mingw-w64`). 35 işlevi derleyip
sınayamadan yazmak, bu deponun bir kez ödediği hatanın aynısı olur.

### Android — çekirdek hazır, ön yüz değil
Android Linux çekirdeği + Bionic libc'dir; POSIX portu NDK ile
(`aarch64-linux-android`) derlenir. **Termux'ta** `kur.sh` olduğu gibi
koşar — `pkg` ve `PREFIX` görülünce `OS=android` deyip
`pkg install clang make` öneriyor.

Gerçek bir **uygulama** için ayrıca: JNI köprüsü ve bir arayüz katmanı
(`gui/` karşılığı). Konsol ön yüzü uygulama içinde anlamsız.

**Doğrulanmayan:** NDK bu kapta yok.

### iOS — port olur, alt süreç olmaz
Altta Darwin/POSIX var, ama App Store kuralları `fork`/`exec`/`popen`/
`system`'i yasaklar. Yani `ses/umay_piper.c` **çıkarılmalı** —
ÖTÜKEN'de zaten çıkarılıyor, mekanizma kurulu. Yuvalar serbest, yani
Ollama yolu çalışır. Konsol ön yüzü yerine arayüz katmanı gerekir.

Ses için iOS'ta doğru yol Piper'ı alt süreç olarak çağırmak değil,
ONNX çıkarımını süreç içinde yapmak ya da ÖTÜKEN'deki gibi kendi
sentezleyicisini kullanmaktır.

**Doğrulanmayan:** Xcode/iOS SDK bu kapta yok.

### ÖTÜKEN — bağlama düzeyinde kanıtlı, koşumda değil
`_start` yazılı, `make otuken-bag` `_start`in tanımlı ve hiçbir
simgenin çözülmemiş olmadığını kanıtlıyor. Ama **ÖTÜKEN üzerinde
koşturulmadı**: çekirdek ağacı, `ulib` ve FAT16 kökü gerekiyor.

## Özet

| Platform | Port | Ön yüz | Bu kapta doğrulanan |
|---|---|---|---|
| Linux | var | konsol | derleme + sınama + kurulum + koşum |
| macOS | var | konsol | katı POSIX derlemesi |
| \*BSD | var | konsol | katı POSIX derlemesi |
| Windows (MSYS2/Cygwin) | var | konsol | — (araç zinciri yok) |
| Windows (yerli) | **yazılacak** | konsol | — (MinGW yok) |
| Android (Termux) | var | konsol | — (NDK yok) |
| Android (uygulama) | var | **yazılacak** | — |
| iOS | **yazılacak** | **yazılacak** | — |
| ÖTÜKEN | var | ÜLGEN | bağlama düzeyi |

Doğrulanmamış bir satıra "çalışır" yazılmayacak.

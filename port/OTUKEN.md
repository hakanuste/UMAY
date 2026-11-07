# UMAY'ı ÖTÜKEN'e bağlamak

UMAY tek başına çalışan bir programdır — Linux'a değil, **POSIX'e**
dayanır (Linux, macOS, *BSD, MSYS2/Cygwin) ve ÖTÜKEN'e bağlanmak
üzere bırakıldı. Bağlanma noktası **tek bir katman**dır: `port/`.

## Değişen tek şey

```
ana bilgisayar        ÖTÜKEN
───────────────       ─────────────────────
port/umay_port_host.c  →  port/umay_port_otuken.c
gui/umay_gui_konsol.c  →  gui/umay_ulgen.c
ses/umay_piper.c       →  (çekirdekteki src/dede.c, syscall ile)
kabuk/umay_cli.c       →  _start (ÖTÜKEN portunun içinde)
```

Çekirdekte (`core/`, `alp/`, `us/`) **tek bir koşullu derleme yoktur**.
Platform farkı ne varsa port katmanındadır.

## Komutlar

    make otuken-denetle     # dikiş sağlam mı: ÖTÜKEN kipinde derlenir
    make otuken OT=../ot    # gerçek yapım: kov/umay.elf

`otuken-denetle` ÖTÜKEN ağacına dokunmaz, yalnız derlemeyi denetler.
Her değişiklikten sonra koşulabilir; dikişin sessizce bozulmasını
engeller.

## Port yüzeyi

Çekirdeğin ÖTÜKEN'den istediği her şey `include/umay_port.h`
içindedir: zaman, günlük, bellek, dosya, IPC, pencere, süreç, yetki,
US motoru, ses, kütüphane kökü. Yaklaşık otuz işlev. Yeni bir platform
eklemek, bu otuz işlevi yazmak demektir.

İki işlev kütüphane yerini belirler ve platform farkını orada tutar:

| işlev | ana bilgisayar | ÖTÜKEN |
|---|---|---|
| `up_kut_kok()` | `UMAY_KUT`, `./kut`, kurulum yeri, `~/.local/share/umay/kut`, `/usr/local/share/umay/kut` | `/` |
| `up_kut_dosya("umay.us1", ...)` | `<kök>/umay.us1` | `/UMAY.US1` (8.3, büyük harf) |

ÖTÜKEN'in FAT16 birimi düz köktür ve adlar 8.3 büyük harftir. Bu bilgi
bir zamanlar çekirdeğin ortasında `#ifdef` ile duruyordu; artık yalnız
portta.

## Çekirdekten beklenenler

ÖTÜKEN tarafında şunlar hazır olmalı (hepsi bugün var):

- Çağrı numaraları `include/syscall.h` ile uyumlu (port 0..35 kullanır)
- Dördüncü argüman **R10**'dan okunur (SYSCALL, RCX'i dönüş adresi için
  kullanır; genel kısıt verilirse çekirdeğe çöp gider — bu yaşanmış
  bir hatadır, port dosyasının başında yazılı)
- `UMAY.ELF`, `UMAY.US1`, `SOZLUK.TXT` FAT16 kökünde
- Çekirdekte `OTUKEN_UMAY_BAGLI` açık (`include/otukenconf.h`)

## Ses

ÖTÜKEN'de Piper koşmaz: ONNX çalışma zamanı ve kayan nokta ister,
ikisi de yok. Orada çekirdeğin kendi formant sentezleyicisi
(`src/dede.c`) devrededir; port `SYS_DEDE_KONUS` ile çağırır.

Piper'ı ÖTÜKEN'e taşımak, VITS çıkarımını tamsayı C'ye yazmak demektir
— US1'de bir kez yapılan işin büyüğü.

## Giriş noktası (_start) — YAZILDI

`umay.ld` `ENTRY(_start)` diyor; karşılığı artık
`port/umay_port_otuken.c` içinde duruyor.

**Önceki durum (ölçülmüş):** `_start` hiçbir dosyada tanımlı değildi ve
`make otuken` yine de **bağlanıyordu** — bağlayıcı yalnız uyarıyordu:

    cannot find entry symbol _start; defaulting to 0000008000a00000

O adreste duran simge `um_bellek_kopya`'ydı. ÖTÜKEN bu ELF'i yükleyip
bir bellek kopyalama işlevine atlardı; UMAY hiç koşmazdı. **ELF'in
bağlanması çalışacağı anlamına gelmez.**

### Ne yapıyor

`_start` üst düzey asm bloğudur — GCC x86-64'te `naked` niteliğini
yoksayar ve giriş/çıkış kodu üretip yığını bozar:

1. `RBP` sıfırlanır — geri iz süren araçlar zincirin sonunu böyle anlar
2. `RSP` 16'ya hizalanır — SysV ABI şartı; yükleyicinin ne bıraktığına
   güvenilmez
3. `umay_otuken_giris()` çağrılır, dönmez

`umay_otuken_giris()` sırayla:

1. **`.bss`'i elle sıfırlar.** `umay.ld` `__bss_basi`/`__bss_sonu`
   simgelerini verir. Yükleyicinin `p_memsz > p_filesz` bölgesini
   sıfırladığına güvenilmez; UMAY'ın bütün sayaçları, indeksleri ve
   "kuruldu mu" bayrakları `.bss`'tedir. Yükleyici zaten sıfırlamışsa
   ikinci kez sıfırlamak zararsızdır.
2. `umay_kur()` — ayar, kütüphane, motor, ajanlar
3. `while (umay_kosuyor()) umay_tur();`
4. `umay_kapan()` — belleği saklar, kanalları kapatır
5. `SYS_EXIT` (5) ile çıkar

Konak karşılığı `kabuk/umay_cli.c`'deki `main()`'dir; o dosya ÖTÜKEN
yapımına girmez (stdio ister), bu yüzden karşılığı port katmanındadır.
Çekirdek (`core/`) ikisini de tanımaz, yalnız yaşam döngüsü
çağrılarını bilir.

### Bir daha kaçmaması için: `make otuken-bag`

`make otuken-denetle` bu hatayı **göremezdi**, çünkü yalnız
`-fsyntax-only` yapar: tanımsız bir simge sözdizimi denetiminden geçer.

`otuken-bag` kapısı ÖTÜKEN ağacı olmadan koşar. Kaynakları nesneye
derler (`-Werror`, serbest duran kip) ve iki şeyi kanıtlar:

1. `_start` **tanımlı** (`nm` ile)
2. **Hiçbir simge çözülmemiş değil** — konak kitaplığı sızmamış.
   Bağlayıcı betiğinin verdiği simgeler `umay.ld`'den **okunur**, elle
   yazılmaz; böylece betikte adı değişen bir simge de yakalanır.

`otuken-denetle` artık bu kapıyı kendisi koşturur.

**Kapı üç bozuk sürümle sınandı, üçünü de yakaladı:**

| Bozma | Sonuç |
|---|---|
| `_start` sökülmesi | `HATA: _start TANIMLI DEGIL` |
| `printf` sızması | `HATA: cozulmemis simge: printf` |
| `umay.ld`'de simge adının değişmesi | `HATA: cozulmemis simge: __bss_basi` |

Sınamanın kendisi de bir kez yanlış kuruldu: `.globl`/etiket satırları
değiştirilip `.type _start, @function` ile `.size _start, . - _start`
yerinde bırakılınca birleştirici `_start` simgesini yine üretti, kapı
da doğru olarak "tanımlı" gördü. Kapı değil sınama hatalıydı.

`make sina` içinde 48. bölüm aynı şeyi kaynak düzeyinde yokluyor
(15 denetim), takım tek başına koşulduğunda da eksiklik görünsün diye.

### Kalan: gerçek donanımda koşturma

`_start` yazıldı ve bağlama düzeyinde kanıtlandı, ama **ÖTÜKEN
üzerinde henüz koşturulmadı.** Bunun için çekirdek ağacı (`OT=...`),
`ulib` ve FAT16 kökü gerekir. Bağlanan bir ELF'in çalıştığını
varsaymamak gerektiğini bu dosya bir kez öğretti; aynısı buraya da
geçerli.

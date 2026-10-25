# UMAY

Türkçe konuşan, çevrim dışı çalışan bir yapay zekâ yardımcısı.
Dede Korkut'un dilinden söyler.

Tek başına kurulan bir programdır. ÖTÜKEN'e bağlı değildir — ne
çekirdek kaynağı, ne bağlayıcı betiği, ne de ÖTÜKEN'e özgü bir koşullu
derleme içinde kalmıştır — ama ÖTÜKEN'e bağlanmak üzere bırakıldı
(`port/`).

## Kurmak

    ./kur.sh                    # derler, sınar, kurar, doğrular
    ./kur.sh --ses              # Piper ses arka ucunu da kurar
    ./kur.sh --onek=/usr/local

Betik işletim sistemini tanır, derleyiciyi ve `make`'i yoklar,
sınamaları koşar, `~/.local`a kurar, PATH'i denetler ve sonunda
**başka bir dizinden çalıştırarak doğrular**. Eksik olan sessizce
geçilmez.

Linux ve macOS'ta kurulur. macOS'ta derleyici için `xcode-select
--install`, Linux'ta `build-essential` yeterlidir.

Kaldırmak:

    make kaldir

## Kullanmak

    umay                                      konuşma kipi (uçbirim)
    umay "bahçede domates nasıl yetişir"      tek soru, yanıtlar, çıkar
    umay --surum
    umay --yardim

Tek soru kipi betiklerden çağırmak içindir. **stdout yanıttır**; tanı,
uyarı ve ilerleme stderr'e gider, böylece boru hattına girer:

    umay "soru" | less
    umay "soru" 2>/dev/null        yalnız yanıt
    umay "soru" 2>gunluk.txt       tanı ayrı dosyaya

`UMAY_GUNLUK_STDOUT=1` eski davranışa döner.

## Ne yapar

- **OTAĞ** — görevleri dağıtan meclis.
- **12 ALP** — uzman ajanlar: AKÇA (ekonomi), YER-SU (permakültür),
  KÜN (güneş enerjisi), DEMİR (elektronik), TULGA (otomotiv), OTACI
  (sağlık), YARGU (hukuk), BALBAL (yapı), ALPAGU (güvenlik), KOPUZ
  (müzik), OYUN (dans), YAZIT (arşiv). Yanında dört görevli: BİLGE,
  GÖZCÜ, ONARICI, YAZMAN.
- **OZAN** — soruyu anlar, niyeti çıkarır, yanıtı Dede Korkut diliyle
  söyler.
- **BİLGİ** — `kut/` altındaki belgeleri alanlara göre indeksler,
  soruya göre dayanak bulur, çelişen kaynakları birleştirmez.
- **BENGİ** — kalıcı bellek; konuşmalar arasında taşınır.
- **US** — çıkarım katmanı, değiştirilebilir arka uçla (aşağıda).
- **Ses** — Piper (sinir ağı tabanlı VITS, çevrim dışı, Türkçe).

Dayanak yoksa motor **hiç çağrılmaz**: "yeterli dayanak yok" denir.
Uydurma kapısı oradadır.

## US — çıkarım arka uçları

Sıra şöyledir ve ilk tutan kullanılır:

| arka uç | ne | ne zaman |
|---|---|---|
| `ollama` | yerelde koşan Ollama | kurulu ve ayaktaysa |
| `stub-ozan` | **alıntı yapar**, üretmez | varsayılan |
| `us1-yerel` | kendi US1 motoru | yalnız `UMAY_US1=1` ile |

Ayarlar: `UMAY_OLLAMA` (adres, varsayılan `127.0.0.1:11434`),
`UMAY_OLLAMA_MODEL`.

**US1 neden varsayılan değil.** Motorun izin verdiği en büyük model
467.808 değişkendir (d=96, 4 katman, ffn=256, 128 imlik sözlük). Gerçek
bir derlemle (3,57 milyon karakter) 120.000 adım eğitildi, doğrulama
kaybı **1,9751** — kendi tavanı. Böyle bir model Türkçenin harf
dizilimini, ünlü uyumunu ve sık sözcük biçimlerini öğrenir; **cümle
kurmaz.** Ürettiği gerçek bir örnek:

    "li hase ve yer bu keyitir, tepa the ın aladığı ne sup şek..."

Bunu "yanıt" diye sunmak, üstüne de güven etiketi koymak yanlış olur.
`stub-ozan` ise üretmez, **bulduğu kaynaktan iki cümle alıntılar**:
dayanak gerçek, metin gerçek, kaynak adıyla anılır. 467 bin değişkenle
yarışması gereken şey budur ve alıntı kazanır.

US1 silinmedi: eğitim hattı (`arac/egit_derlem.py`), motor
(`us/us1.c`) ve model (`kut/umay.us1`) yerinde duruyor ve ÖTÜKEN
içinde — Ollama'nın olmadığı yerde — dil katmanı olarak anlamını
koruyor. Değişen tek şey, anlamsız metni yanıt diye sunmamak.

Kendi modelini eğitmek:

    make egit [ADIM=120000]        # kontrol noktasından SÜRDÜRÜR

Eğitim her ölçümde `kontrol.npz` yazar (float32 ağırlıklar, Adam
momentleri, adım sayacı): kesilirse ilerleme kaybolmaz. `--sifirdan`
baştan başlar.

## Ses

    ./ses/piper_kur.sh

İki yol vardır ve ikisi de çalışır:

1. **Piper ikilisi** — `piper` yoldaysa kullanılır.
2. **Doğrudan ONNX** — ikili yoksa model `ses/soylet.py` ile koşar:
   metin → espeak-ng IPA fonemleri → eşleme → ONNX → dalga.
   Gereken: `python3`, `onnxruntime`, `espeak-ng`, `.onnx` ve
   `.onnx.json`. Piper ikilisi her dağıtımda paketli değil; bu yol onu
   gereksiz kılar.

**Sahte başarı yok.** Ses modeli, çalıcı ya da çalışan bir motor —
biri bile eksikse ses sürücüsü AÇILMAZ, günlüğe neden yazılır ve UMAY
yalnız yazıyla yanıtlar. Metni günlüğe basıp "konuştum" demek yankı
yapmaktır. Dört hata yolu da sınanıyor.

| değişken | ne |
|---|---|
| `UMAY_PIPER` | piper çalıştırılabiliri |
| `UMAY_PIPER_SES` | `.onnx` ses modeli |
| `UMAY_PIPER_AYAR` | `.onnx.json` (verilmezse `<model>.json`) |
| `UMAY_PIPER_PY` | `soylet.py` yolu |
| `UMAY_PIPER_UZUN` | konuşma ağırlığı; Dede Korkut **1.52** (büyük = yavaş) |
| `UMAY_PIPER_RENK` | ses rengi düzeltmesi (öntanımlı KAPALI) |
| `UMAY_SES_OYNAT` | çalıcı komutu (`afplay`, `paplay`, `aplay`, …) |

`arac/ses_rengi.py` bir hedef kaydın ölçülen perde kuşağını ve tayf
dengesini çıkarır; `UMAY_PIPER_RENK` ile uygulanır. Bir **ses klonu
değildir**: belirli bir insanın ses kimliğini taşımaz, yalnız ölçülen
akustik niteliklere oturtur. Öntanımlı kapalıdır — ölçüte yakınlık
kulağa iyilik demek değil.

## Kütüphane

**Kütüphane içeriği depoda yoktur.** `kut/` altında yalnız üç örnek
belge ve eğitilmiş model bulunur; kitap ve el kitabı metinlerinin yeri
bir kaynak deposu değildir. Kendi arşivini sen kurarsın:

    make kitaplik ARSIV=/yol/Ham_Arsiv

Künyeyi klasör adından yazar, PDF gürültüsünü ve yazıtipi
bozulmalarını temizler, büyük belgeleri parçalara böler, dilini sezer.
Ayrıntı: `arac/KITAPLIK.md`.

UMAY veriyi şu sırayla arar: `UMAY_KUT`, çalışma dizinindeki `kut/`,
kurulumda gömülen yer, `~/.local/share/umay/kut`,
`/usr/local/share/umay/kut`. Bir aday yalnızca DİZİN olduğu için
seçilmez; içinde gerçekten kütüphane bulunmalıdır (`umay.us1` ya da
`ortak/`). Yoksa önceki bir koşudan kalma boş bir `kut/` düzgün
kurulumu gölgeliyor ve program sessizce yedek motora düşüyordu.

Her belgenin başında künye olmalı:

    #kategori: otaci
    #baslik:   Temel İlk Yardım
    #etiket:   ilkyardim,travma
    #kaynak:   Saglik Bakanligi
    #dil:      tr

Künye yoksa belge "ortak" havuza düşer ve 12 uzmanın hiçbiri göremez.

## Dil — Türkçe olmayan kaynaklar

Künyedeki `#dil` boşsa `tr` varsayılır. **Dayanak Türkçe değilse UMAY
bunu söyler:**

    Dayanağım İngilizce bir kaynak; alıntı özgün dilindedir,
    çevirisi benim değildir.

ve kaynak satırında hangi dayanağın yabancı olduğu görünür. Türkçe
dayanakta böyle bir not düşülmez: her yanıta dil notu koymak gürültü
olur ve uyarıyı değersizleştirir.

Kütüphaneyi bir kez çevirmek:

    make ceviri-say                       # ne kadar iş var
    make ceviri MODEL=qwen2.5             # Ollama ile
    make ceviri MODEL=qwen2.5 EN_COK=20   # önce azıyla dene

Çeviri bir **kurulum adımıdır**, sorgu anında yapılmaz: aynı parça
yüzlerce kez çevrilmesin, gecikme çevirmene bağlanmasın ve en önemlisi
**denetlenebilsin** — bir kez çevirip diske yazarsan yanlışı görüp elle
düzeltebilirsin. Özgün metin yerinde kalır; çeviri `<ad>-tr.txt` olarak
yanına yazılır ve künyesinde `#ozgun_dil` ile `#ozgun` durur.

Çevirmen yoksa, çıktı boşsa, çok kısaysa ya da **hâlâ yabancı dilde
görünüyorsa** parça atlanır ve sayılır. Çevrilmemiş parçayı "çevrildi"
diye yazmak, yanlış dilde kaynak vermekten kötüdür.

## ÖTÜKEN'e bağlamak

Bağlanma noktası tek bir katmandır: `port/`. Çekirdekte (`core/`,
`alp/`, `us/`) tek bir koşullu derleme yoktur.

    make otuken-denetle      # dikiş sağlam mı (ÖTÜKEN ağacı gerekmez)
    make otuken OT=../ot     # gerçek yapım -> kov/umay.elf

`otuken-denetle` her değişiklikten sonra koşulabilir; dikişin sessizce
bozulmasını engeller. Ayrıntı: `port/OTUKEN.md`.

## Sınamak

    make sina         # UMAY sınamaları + US1 motor sınaması
    make sina-us1     # yalnız motor

**223 + 10 sınama.** Motor sınaması, C sabit nokta motorunu Python
referansıyla sayısal olarak karşılaştırır: aynı nicelenmiş ağırlıklar
iki tarafta da okunur, sapma niceleme payının içinde kalmalıdır
(ölçülen: 0,0933 / genlik 21,59).

Sınanan şeyler arasında, her biri gerçekten yaşanmış bir hatanın
karşılığı olarak: ses aygıtı yokken sürücünün açılmaması, hiçbir yol
kalmadığında "konuştum" denmemesi, motorun en büyük modelinin
yüklenebilmesi, kırpılmış modelin reddedilmesi, aynı belgenin iki
parçasının çatışma sayılmaması, Türkçe olmayan dayanağın bildirilmesi.

## Lisans

Henüz seçilmedi. Lisans dosyası olmadan başkaları bu kodu yasal olarak
kullanamaz; kendine saklamak istiyorsan bu zaten doğru durumdur.

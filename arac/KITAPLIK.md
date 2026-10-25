# Ham arşivi UMAY kitaplığına çevirmek

    python3 arac/kitaplik_kur.py <ham_arsiv> kut

Klasör adından alanı çıkarır, PDF çıkarım gürültüsünü temizler,
belgeleri parçalara böler, her parçanın başına künye yazar.

## Neden gerekli

UMAY'ın BİLGİ katmanının dört sınırı vardı; dördü de bu arşiv
denenince görüldü:

| sınır | sonuç | durum |
|---|---|---|
| tarayıcı dizini okumuyordu, alan başına 6 sabit ad yokluyordu | belgeler hiç görünmüyordu | `up_dizin_listele` eklendi, tarayıcı gerçekten tarıyor |
| listeleme sayfalanmıyordu | 1012 parçanın 258'i indekslendi | sayfalama eklendi |
| belgelerde `#kategori:` künyesi yoktu | hepsi "ortak" havuza düşüyordu | araç künye yazıyor |
| bir kayıt en çok 3072 bayt | 348 KB'lik belge ilk sayfasına kırpılıyordu | belge parçalara bölünüyor |

## Künye biçimi

    #kategori: otaci
    #baslik:   Temel İlk Yardım (3/64)
    #etiket:   ilkyardim,travma
    #kaynak:   01_Saglik-Bakanligi-Ilk-Yardim.txt

Kategori **dosya yolundan çıkarılmaz**; belgenin ilk satırları esastır.

## Alan eşlemesi

Klasör adındaki anahtar sözcükten bulunur: mimari/kerpiç → `balbal`,
biyodizel/mercedes/musso → `tulga`, telsiz/çark → `demir`,
sera/su/kompost/kümes → `yer-su`, radar/kamera/siber → `alpagu`,
tıp/ilk yardım → `otaci`, sahne/folklor → `oyun`, kültür/tarih/dil →
`yazit`. Eşleşme yoksa `ortak`.

Eşlemeyi `ESLEME` listesinden değiştirebilirsin.

## Dokunmadıkları

- `00_Onay_Bekleyenler/` — insan onayından geçmemiş ham döküm.
- `99_Kisisel_Saglik_Korumali/` — kişisel sağlık verisi.

İkisi de hiç okunmaz. Arşivin kendi MANIFEST'i de böyle diyor
(`"saglik_indekse_dahil": false`).

## İndeks sınırı

`BILGI_KAYIT_MAX`: ana bilgisayarda 4096, ÖTÜKEN'de 96 (ELF bellek
payı dar). Sınır dolarsa günlüğe açıkça yazılır — sessizce kesilmez.

## Künyede dil  (`#dil:`)

Her parçanın dili sezilip künyeye yazılır. Sezme, işlev sözcükleri ve
Türkçeye özgü harflerle yapılır; Türkçe lehine belirgin bir üstünlük
yoksa `belirsiz` denir. **Bilinmeyeni Türkçe saymak yanlış olurdu** —
ajan o zaman yabancı dildeki alıntıyı Türkçe sanar ve kaynağı gizler.

Ölçülen gerek: bir arşivde 1504 parçanın 503'ü (%33,4, 1,14 milyon
karakter) İngilizce çıktı ve neredeyse tamamı tek alanda toplanmıştı
(araç el kitapları). Kullanıcı Türkçe soruyor, ajan İngilizce metin
getiriyor ve bunu söylemiyordu.

Çeviri için: `arac/cevir.py` ya da `make ceviri`. Çeviri bir kurulum
adımıdır, sorgu anında yapılmaz; özgün metin yerinde kalır.

## Yazıtipi bozulmaları

PDF'ler gömülü alt küme yazıtipi kullanır; çıkarıcının o yazıtipinin
kendi eşlemesini yanlış okuması Türkçe harfleri başka imlere çevirir.
Ölçülen: 8493 sözlük dışı im (%0,238), 342 ayrı işaret.

Eşleme **tahminle değil bağlamdan** doğrulandı:

| im | doğrusu | kanıt | sayı |
|---|---|---|---|
| U+0122 | `ş` | "hazırlanmıĢ" → hazırlanmış | 1701 |
| U+0120 | `İ` | "MĠLLÎ EĞĠTĠM" → MİLLÎ EĞİTİM | 220 |
| U+0121 | `Ş` | "GĠRĠġ" → GİRİŞ, "KOġUL" → KOŞUL | 116 |

İlk tahmin **yanlıştı**: U+0121 için küçük `ı` sanılmıştı, küçük harf
görüntüsü yanıltıcı. Bağlama bakılınca hepsinin büyük harfli
başlıklarda olduğu ve `Ş` olduğu görüldü. Tek bayt kodlama çevrimleri
de denendi (cp1254, iso8859-9/-4/-13, cp1257, mac_turkish, cp857);
hiçbiri bu imleri üretmiyor — yani kodlama karışıklığı değil.

U+00DE **harf değildir**: İngilizce kılavuzda madde imi olarak geçer,
harfe çevirmek metni bozardı. Yazıtipinin özel kullanım alanından
gelen imler (U+F0B7, U+F0D8, U+F0D7, U+F03D, U+F061) Symbol/Wingdings
glifleridir, karşılıklarına çevrilir.

Sonuç: sözlük dışı im 8493 → 2318, kapsam %99,930 → %99,952, bozulmuş
harf 2244 → 0. Ve sözlükte artık Türkçenin bütün harfleri var.

**Hâlâ duran bir kusur:** bazı belgelerde harf tümden düşmüş ve yerine
boşluk kalmış — "T bbi" 108 kez (Tıbbi), "T rkler" 97 kez (Türkler),
"S merlilerde" 97 kez. Bunlar başlıklarda olduğu için aramayı doğrudan
bozuyor. Düzeltmesi tahminle yapılmamalı: doğrusu eksik harfi deneyip
sonucun derlemde başka yerde geçen gerçek bir sözcük olup olmadığına
bakmaktır. Henüz yapılmadı.

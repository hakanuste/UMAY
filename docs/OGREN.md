# UMAY — kendi kendine öğrenme (web araştırması)

## "Eğitim" burada ne demek

Büyük dil modelinin **ağırlıkları cihazda güncellenmez**; bunun için yüzlerce
GB bellek ve günlerce hesap gerekir. UMAY'ın öğrenmesi şudur:

1. Dayanak bulamadığı soruyu **boşluk** olarak kaydeder
   (`<kütüphane>/_ogren/bosluk.txt`).
2. Boşluğu web'de araştırır: önce Vikipedi (Türkçe), bulamazsa DuckDuckGo özeti.
3. Bulduğunu **künyeli belge** olarak kütüphaneye yazar
   (`<kütüphane>/<alan>/web-<özet>-<n>.txt`) ve aynı anda dizine ekler.
4. Bir sonraki soruda bu parça **dayanak** olur; dil modeli onu okur, uydurmaz.

Elle konan belgelerden ayırt edilir: dosya adı `web-` ile başlar, kaynak türü
"dış kayıt"tır ve kaynak satırında **"doğrulanmamış"** yazar. Yanıtta bu kaynak
adıyla anılır.

## Ne zaman çalışır

| Durum | Ne olur |
|---|---|
| Soruya dayanak yok (BİLGE ya da bir uzman) | Boşluk kaydedilir, **hemen** araştırılır, ikinci aramada yanıta girer |
| Konuşma kipinde boşta | Her ~2 dakikada bir bekleyen boşluk işlenir |
| `umay --ogren` | Bekleyen boşlukların tamamı (en çok 25) işlenir |
| `umay --ogren "konu"` | O konu araştırılır |
| Ağ yok | Boşluk **beklemeye** kalır, sahte başarı yoktur; ağ gelince tamamlanır |
| Web'de bir şey bulunamadı | Konu bir kez denenir, `bitti.txt`'ye yazılır; her soruda yeniden aranmaz |

Bir oturumda en çok 40 anlık araştırma yapılır; iki ağ çağrısı arası en az 4 sn.

## Gizlilik

Araştırma sırasında cihazdan **yalnız soru anahtar sözcükleri** çıkar
(durak sözcükleri atılmış, en çok 5 sözcük). Kapatmak için:

    UMAY_OGREN=0

Kapalıyken ne ağa çıkılır ne de `_ogren/` dosyaları yazılır.

## Güvenlik

- Web metni yalnız **veri**dir. Künye satırı (`#kategori:` vb.) taklit edemez:
  kategori, başlık, kaynak ve dil dosyayı UMAY yazarken **kendisi** belirler;
  gövde `#` ile başlıyorsa etkisizleştirilir (sınanır).
- Ağa çıkış tek yerdedir: `up_ag_getir()` (port). Yalnız `http(s)`, yönlendirme
  en çok 3 ve yalnız `https`'e, en çok 1 MB, 10 sn. Adres, kabuğa verilmeden
  önce yalnız güvenli karakterlere karşı denetlenir.
- Doğrulanmamış web kaynağı, doktrin ve elle konan belgelerin yerini tutmaz;
  güven düzeyi yanıtta ayrıca gösterilir.

## Platformlar

| Platform | Ağ yolu |
|---|---|
| Linux, macOS, Windows 10+, Termux | `curl` (hazır gelir; Termux'ta `pkg install curl`) |
| Android/iOS uygulaması (gömülü) | **Uygulama getirir**: `Umay.agGetiriciAta(...)` / `umay_gomulu_ag_ata(...)` |
| ÖTÜKEN | Henüz ağ yok; modül sessizce kapalı kalır |

`curl` yoksa (ya da gömülü uygulama getirici takmadıysa) öğrenme kapalıdır ve
`umay --ogren` bunu söyler.

## Ortam değişkenleri

| Değişken | Anlamı |
|---|---|
| `UMAY_OGREN=0` | Öğrenmeyi tamamen kapatır |
| `UMAY_AG_WIKI` | Vikipedi taban adresi (varsayılan `https://tr.wikipedia.org`) |
| `UMAY_AG_DDG` | DuckDuckGo HTML adresi (varsayılan `https://html.duckduckgo.com/html/`) |

## Sınır: ne **sınanmadı**

Bu geliştirme ortamında web'e çıkış yoktur. Ayrıştırıcılar ve bütün akış
**sahte sunucu/getirici** ile sınandı (297 sınama; Linux, Windows/Wine, ARM64).
Gerçek Vikipedi ve DuckDuckGo yanıtlarıyla **ilk koşu sizin elinizden
çıkacak**; DuckDuckGo biçimini değiştirirse özet ayrıştırıcısı güncellenmelidir
(Vikipedi'nin API'si kararlıdır).

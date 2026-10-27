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
**sahte sunucu/getirici** ile sınandı (343 sınama; Linux, Windows/Wine, ARM64).
Gerçek Vikipedi ve DuckDuckGo yanıtlarıyla **ilk koşu sizin elinizden
çıkacak**; DuckDuckGo biçimini değiştirirse özet ayrıştırıcısı güncellenmelidir
(Vikipedi'nin API'si kararlıdır).

---

# Planlı araştırma, bekçi, karne ve kapılı eğitim

OpenJarvis'in *fikirlerinden* (sınırlı bütçeli araştırma döngüsü, döngü bekçisi,
izleme → öğrenme → ölçüm kapısı) esinlenilerek UMAY'ın kendi mimarisinde, sıfırdan
C ile yazıldı. OpenJarvis'ten kod alınmadı.

## ARAŞTIRMACI ajanı (`alp/alp_arastirmaci.c`, `core/umay_arastir.c`)

"araştır …" ile açılan görevler bu ajana gider. Model çağırmaz; belirlenimlidir.

1. **Plan:** komut sözcükleri ("araştır", "derle"…) atılır; soru açılara bölünür
   (hepsi birlikte + en ayırt edici tek sözcükler).
2. **Arama:** her açı kütüphanede aranır. **Bütçe 5.** Her çağrıdan önce bekçiye sorulur.
3. **Genişletme:** kapsam %100 değilse ve web açıksa konu web'den öğrenilir, tam açı yeniden aranır.
4. **Çapraz denetim:** her anahtar sözcüğün kaç *ayrı* kaynakta geçtiği sayılır;
   güvenilen kaynaklar çelişiyorsa işaretlenir.
5. **Rapor:** numaralı bulgular `[1] [2]…`, kapsam, eksik terimler, güven ve nedeni,
   kaynak listesi. Rapordaki her cümle bir kaynaktan **aynen** alınır; kaynak
   yoksa "uydurmam" der. Yalnız doğrulanmamış web varsa güven en çok **orta**.

Komut satırı: `umay --arastir "soru"`.

## BEKÇİ (`core/umay_bekci.c`)

Araç çağrısı döngülerini engeller: aynı çağrı en çok 3 kez, araç başına bütçe,
A-B-A-B-A-B gidip gelme. **Önce uyarır, yinelenirse engeller.**

## KARNE ve KAPILI EĞİTİM (`core/umay_egit.c`)

Her yanıt (ajan, soru, güven, dayanak sayısı) yerel `_ogren/karne.txt` dosyasına yazılır
(`umay --karne` ajan başına özet verir). **Eğitim turu** (`umay --egit`; konuşma
kipinde boştayken 5 dakikada bir kendiliğinden):

1. **Ölç:** karnedeki son ≤40 soru kütüphanede ne kadar karşılanıyor (web'siz, binde).
2. **Öğren:** zayıf sorulardan çıkan boşluklar web'den kapatılır.
3. **Yeniden ölç.**
4. **Kapı:** ortalama kapsam en az **+%2** artmalı **ve** önceden karşılanan soruların
   en az **%85**'i hâlâ karşılanmalı. Aksi hâlde tur **geri alınır**: eklenen kayıtlar
   dizinden atılır, yazılan web dosyaları silinir.

Dil modelinin ağırlıkları değişmez; "eğitilen" şey kütüphanedir.

## Sınırlar

- Ajan *seçimi* karneden öğrenmez; karne şimdilik ölçer ve eğitim turunu yönlendirir.
- Kapı ölçüsü kütüphane kapsamıdır, yanıt kalitesi değil (yanıtı bir model üretiyorsa
  ayrıca ölçülmelidir).

## 5N1K — her uzman altı soruyu yanıtlar

Soru türü (`bilgi_soru_tipi`): ne, nerede, ne zaman, neden, nasıl, kim (+ kaç).
Tür, yanıt cümlesinin seçimini yönlendirir ("nasıl" → yapılır/önce/sonra; "ne zaman" → mevsim/ay/saat ...).

**"Nasıl yapılır" → konunun ajanı.** Anahtar sözcük alan bulamazsa `ozan_coz`, bilgi tabanındaki en uygun kaydın
kategorisinden alanı bulur; konunun uzmanı çalışır, genel ajan değil.

**Denetim:** `umay --5n1k` 12 uzman × 6 tür matrisi basar. Boş hücre = o uzmanın konularında, soru türünün
ipucunu taşıyan kaynaklı yanıt yok. Boşluklar web-öğrenme sırasına yazılır (`umay --ogren`); uydurma yanıt
üretilmez — özellikle tıp/hukuk gibi alanlarda boşluk kaynakla (web ya da belgen) kapanır.

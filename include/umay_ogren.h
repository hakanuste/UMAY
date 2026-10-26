/* =====================================================================
 * ÖĞREN — UMAY'ın kendi boşluğunu görüp kapatması.
 *
 * "EĞİTİM" NEYDİR (dürüst tanım)
 *   Büyük dil modelinin AĞIRLIKLARI cihazda güncellenmez; bu, birkaç
 *   yüz GB bellek ve günlerce hesap ister. UMAY'ın öğrenmesi şudur:
 *     1) cevaplayamadığı soruyu BOŞLUK olarak kaydeder,
 *     2) boşluğu web'den araştırır (önce Vikipedi, sonra DuckDuckGo),
 *     3) bulduğunu künyeli belge olarak kütüphaneye yazar ve hemen
 *        dizine ekler; bir sonraki soruda dayanak olur.
 *   Dil modeli bu bilgiyi DAYANAK olarak okur; uydurmaz.
 *
 * GÜVEN
 *   Web'den gelen her belge "doğrulanmamış web" diye işaretlenir,
 *   kaynak türü KT_DIS'tır ve dosya adı "web-" ile başlar. Doktrinin
 *   ve elle konan belgelerin yerini tutmaz.
 *
 * GİZLİLİK
 *   Araştırma sırasında YALNIZ soru anahtar sözcükleri cihazdan çıkar.
 *   Kapatmak için ortam değişkeni: UMAY_OGREN=0
 *
 * DIŞ BAĞ
 *   Ağa çıkış tek yerdedir: up_ag_getir() (port). ÖTÜKEN portunda
 *   ağ yoktur; modül orada sessizce UM_YOK döner.
 * ===================================================================== */
#ifndef UMAY_OGREN_H
#define UMAY_OGREN_H
#include "umay.h"

/* Bir adresi getirir; gövdeyi cik'a yazar, bayt sayısını döner (<0 hata).
 * Sınamada gerçek ağ yerine sahte getirici takılır. */
typedef int64_t (*ogren_getirici_t)(const char *url, char *cik, uint32_t n);
void ogren_getirici_ata(ogren_getirici_t f);       /* 0 = portun getiricisi */

int  ogren_acik(void);                             /* 1 = öğrenme açık */

/* Cevapsız soruyu kaydeder. UM_OLDU: yeni kayıt; UM_YOK: zaten vardı
 * ya da öğrenme kapalı. Kategori bilinmiyorsa "ortak" olur. */
int  ogren_bosluk_kaydet(const char *kategori, const char *soru);

/* Konuyu web'de arar, parçalar halinde kütüphaneye yazar ve dizine
 * ekler. Dönüş: eklenen parça sayısı (>0), 0 = bulunamadı/zaten
 * öğrenilmişti, UM_YOK = ağ yok/kapalı, UM_YANLIS = konu boş. */
int  ogren_arastir(const char *kategori, const char *konu);

/* Soru sırasında anında öğrenme (hız sınırlı). Dönüş ogren_arastir gibi. */
int  ogren_hemen(const char *kategori, const char *soru);

/* Bekleyen boşlukları en çok "en_cok" tane işler. Dönüş: eklenen parça. */
int  ogren_gecis(int en_cok);

/* Boştayken çağrılır; seyrek aralıkla bir boşluk işler. */
void ogren_bosta(void);

/* Durum: bekleyen boşluk sayısı, öğrenilmiş parça sayısı (oturum). */
int  ogren_bekleyen(void);
int  ogren_oturum_parca(void);

/* --- sınanabilir saf işlevler --- */
/* JSON'daki ilk "anahtar":"değer" dizgesini çözer (\n, \uXXXX, vekil çiftler). */
int  ogren_json_dizge(const char *json, const char *anahtar, char *cik, uint32_t n);
/* Etiketleri atar, varlıkları (&amp; &#39; ...) çözer, boşlukları sadeleştirir. */
uint32_t ogren_html_temizle(const char *kaynak, uint32_t uzun, char *cik, uint32_t n);
/* Metni ≤ en_cok bayt, UTF-8 ve paragraf/cümle sınırında parçalara böler.
 * Her parçanın başlangıç ofsetini ve uzunluğunu yazar; parça sayısını döner. */
int  ogren_parcala(const char *metin, uint32_t en_cok, uint32_t *bas, uint32_t *uz, int en_cok_parca);
/* UTF-8 yüzde-kodlama (sorgu dizgesi için). */
uint32_t ogren_url_kodla(const char *s, char *cik, uint32_t n);
/* Basit dil kestirimi: "tr" ya da "en". */
const char *ogren_dil_kestir(const char *metin);
#endif

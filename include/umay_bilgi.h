/* =====================================================================
 * BİLGİ — UMAY belge deposu (kaynak doküman + RAG).
 *
 * DEPO STANDARDI
 *   Kök: /kut/umay/
 *   12 uzman alanı + ortak havuz, her biri kendi dizininde:
 *     akca yer-su kun demir tulga otaci yargu balbal alpagu kopuz oyun yazit
 *     ortak            (birden çok alanı ilgilendiren belgeler)
 *
 *   KATEGORİ DOSYA YOLUNDAN ÇIKARILMAZ. Her belgenin başındaki üstveri
 *   esastır:
 *       #kategori: balbal
 *       #baslik:   Horasan harcı karışımı
 *       #etiket:   harc,sarnic
 *       #kaynak:   Usta notları 1998
 *       <boş satır>
 *       ...gövde...
 *   Üstveri yoksa belge "ortak" havuzuna düşer ve günlüğe yazılır.
 * ===================================================================== */
#ifndef UMAY_BILGI_H
#define UMAY_BILGI_H
#include "umay.h"

/* Indeks boyu yapim zamani ayaridir.
 * ÖTÜKEN'de UMAY.ELF'in bellek payi dar (ELF sayfa siniri); ana
 * bilgisayarda boyle bir sinir yok. 96 kayit x 3 KB = 288 KB ile
 * 2.5 milyon karakterlik bir arsivin ancak %10'u indekslenebiliyordu.
 * Parcalanmis bir kitaplikta bin dolayinda parca olur. */
#ifndef BILGI_KAYIT_MAX
#ifdef UMAY_OTUKEN
#define BILGI_KAYIT_MAX   96
#else
#define BILGI_KAYIT_MAX   4096
#endif
#endif
#define BILGI_KELIME_MAX  96
#define BILGI_ICERIK_MAX  3072
#define BILGI_ALINTI_MAX  800
#define BILGI_ETIKET_MAX  4
#define BILGI_ALAN_SAYI   13          /* 12 uzman alanı + ortak */

/* Kaynak türü — BENGİ kaydı ile kaynak belge asla karışmasın diye. */
typedef enum {
    KT_DOKTRIN = 0,   /* ikiliye gömülü proje veri tabanı */
    KT_BELGE,         /* /kut/umay altındaki dosya        */
    KT_DIS,           /* çalışırken dışarıdan katılan     */
    KT_BENGI          /* BENGİ kaydı — BİLGİ'ye GİREMEZ   */
} kaynak_tur_t;

/* BELGENİN DİLİ
 * NEDEN GEREKLİ: kitaplığın %33.4'ü (503 parça, 1.14 milyon karakter)
 * İNGİLİZCEDİR ve neredeyse tamamı "tulga" alanındadır (524 parçanın
 * 486'sı: Musso ve M104 el kitapları). Kullanıcı Türkçe soruyor, ajan
 * İngilizce metin getiriyordu ve bunu SÖYLEMİYORDU.
 *
 * Dil, kaydın künyesinden okunur (#dil:). Boşsa "tr" varsayılır —
 * eski kayıtlar Türkçedir. Ajan, Türkçe olmayan bir kaynaktan
 * yanıtlıyorsa bunu AÇIKÇA bildirir; sessizce başka dilde alıntı
 * vermek, kaynağı gizlemektir. */
#define BILGI_DIL_MAX 8

typedef struct {
    char kategori[32];
    char baslik[96];
    char kaynak[96];                     /* dosya yolu ya da kaynak adı */
    kaynak_tur_t kaynak_tur;
    char dil[BILGI_DIL_MAX];             /* "tr", "en", ... */
    char ozgun_dil[BILGI_DIL_MAX];       /* çeviri ise özgün dili */
    char etiket[BILGI_ETIKET_MAX][24];
    int  etiket_sayi;
    char alinti[BILGI_ALINTI_MAX];
    int  skor;
} baglam_t;

typedef struct {
    const char *const *kategoriler;
    int         kategori_sayi;
    const char *etiket;
    const char *kaynak;
} bilgi_suzgec_t;

/* Çatışma: aynı kategoride, ayrı kaynaklardan, yakın skorlu iki kayıt.
 * Sessizce birleştirilmez; OTAĞ'a bildirilir. */
typedef struct {
    int  var;
    char kategori[32];
    char kaynak_a[96];
    char kaynak_b[96];
    char baslik_a[96];
    char baslik_b[96];
} catisma_t;

const char *bilgi_alan_kategorisi(alan_t a);   /* ALAN_TULGA -> "tulga" */
const char *bilgi_alan_dizini(int i);          /* 0..12 depo dizin adları */

int  bilgi_dizin_kur(const char *kok);         /* 13 alt dizini açar */
int  bilgi_doktrin_yukle(void);
int  bilgi_tara(const char *kok);              /* üstveriye göre indeksler */
int  bilgi_sayi(void);
void bilgi_kirp(int n);                        /* son eklenenleri geri al */
int  bilgi_kategori_sayisi(const char *kategori);

int  bilgi_kayit_kat(const char *kategori, const char *baslik, const char *kaynak,
                     const char *etiketler, const char *icerik);
int  bilgi_kayit_kat_tur(const char *kategori, const char *baslik, const char *kaynak,
                         kaynak_tur_t kt, const char *etiketler, const char *icerik);

/* Dili bilinen kayit. Kitaplik yukleyicisi bunu kullanir; dil
 * verilmezse "tr" varsayilir. */
int  bilgi_kayit_dil(const char *kategori, const char *baslik, const char *kaynak,
                     kaynak_tur_t kt, const char *etiketler, const char *icerik,
                     const char *dil, const char *ozgun_dil);

int  bilgi_baglam_bul(const char *soru, baglam_t *cikan, int en_cok);
int  bilgi_baglam_bul_kat(const char *soru, const char *const *kategoriler,
                          int kat_sayi, baglam_t *cikan, int en_cok);
int  bilgi_baglam_bul_suzgec(const char *soru, const bilgi_suzgec_t *s,
                             baglam_t *cikan, int en_cok);
int  bilgi_catisma_bul(const baglam_t *dizi, int sayi, catisma_t *c);

/* Soruyu/metni durak sozcukleri atilmis, Turkce kucultulmus sozcuklere
 * ayirir. Donen deger sozcuk sayisidir. bengi katmani da kullanir:
 * ayirici ve durak listesi TEK yerde durmali — iki ayri ayirici
 * yazilsa biri degisince oteki sessizce sapar. */
int bilgi_kelime_ayikla(const char *metin, char cikan[][24], int en_cok);

#endif

#ifndef UMAY_GOMULU_H
#define UMAY_GOMULU_H
/* UMAY'I BIR UYGULAMANIN ICINE GOMMEK (Android, iOS, Windows/macOS uygulamalari).
 *
 * Telefonlarda "umay" komutu calistirilamaz; uygulama UMAY'i kitaplik olarak
 * baglar. Yanit ekrana BASILMAZ, cagirana dondurulur; sesi uygulama kendi
 * konusma motoruyla (Android TextToSpeech, iOS AVSpeechSynthesizer, Windows
 * SAPI) okur. Bu yuzden Piper/onnxruntime gerekmez.
 *
 * Kullanim sirasi: baslat -> sor (istedigin kadar) -> kapat.
 * IPLIK: tek iplikten cagrilir; es zamanli cagri DESTEKLENMEZ. */
#include <stdint.h>

/* veri_dizini: icinde umay.us1 ve alan klasorleri (kut/) olan dizin; YAZILABILIR
 * olmali (BENGI hafizasi oraya saklanir). 0 verilirse varsayilan arama sirasi. */
int         umay_gomulu_baslat(const char *veri_dizini);   /* 0 = oldu */

/* Yaniti `cevap`a yazar (UTF-8, sonu NUL). Dondurdugu: yazilan bayt sayisi,
 * hata ise negatif. Tampon kucukse yanit KIRPILIR ve dondurulen deger
 * kirpilmis uzunluktur. */
int         umay_gomulu_sor(const char *soru, char *cevap, uint32_t n);

/* ---- WEB ARASTIRMASI / KENDI KENDINE OGRENME ----
 * Telefonda "curl" yoktur; ag cagrisini UYGULAMA yapar (Android
 * HttpURLConnection, iOS URLSession) ve sonucu UMAY'a verir. Uygulama bir
 * getirici takmazsa web arastirmasi KAPALI kalir, sahte basari yoktur.
 *
 * getirici: `url`yi getirir, govdeyi cevap'a (NUL'lu, en cok n-1 bayt)
 * yazar, bayt sayisini dondurur; hata/ag yoksa negatif. Soru/arastirma
 * ayni iplikten cagrilir, getirici de o iplikten cagrilir. */
typedef int64_t (*umay_ag_getirici_t)(const char *url, char *cevap, uint32_t n);
void        umay_gomulu_ag_ata(umay_ag_getirici_t getirici);

/* konu != 0: konuyu arastirip kutuphaneye ekler. konu == 0: bekleyen
 * (yanitlanamamis) sorulari en cok 5 tane isler. Dondurulen: eklenen
 * parca sayisi; negatif = ag yok/kapali. Bloke eder: arka planda cagirin
 * (ayni iplik kurali: sor ile ESZAMANLI cagirmayin). */
int         umay_gomulu_ogren(const char *konu);

/* Etkin çıkarım motorunun adı: "llama" = gömülü gerçek model; "stub-ozan" = modelsiz yedek.
 * Uygulama bunu kullanıcıya GÖSTERMELİ: yedek motor gerçek model değildir. */
const char *umay_gomulu_motor(void);

/* <veri>/gelen/ içindeki yeni belgeleri (.txt .md .pdf) ajanlara inceletir ve yerleştirir.
 * Dönen: yazılan parça sayısı; rapor okunur özettir. Bloke eder: arka planda, sor ile ESZAMANLI değil. */
int         umay_gomulu_kabul(char *rapor, uint32_t n);

/* ANIMSATICI / ILAC ALARMI: uygulama ~20 sn'de bir cagirir, donen satirlari kendi bildirimiyle gosterir
 * (satirlar 0x1E karakteriyle ayrilir; satirin icinde '\n' olabilir — sabah bildirisi cok satirlidir; tampon en az 12288 bayt). Donen: vakti gelen satir sayisi; negatif = saat okunamiyor/hazir degil.
 * Kayit yalniz cihazdaki <veri>/animsa.txt dosyasindadir. Onay ("icitim") sohbetten gelir: umay_gomulu_sor. */
int         umay_gomulu_anim_yokla(char *cikti, uint32_t n);

/* ---- KASA (sifreli ozel kutukler; bkz. umay_kasa.h) ----
 * Ana parola SOHBETTEN GECMEZ: uygulama ayri bir gizli giris alani gostermeli. Donus kodlari umay_kasa.h'deki gibi.
 * umay_gomulu_baslat'tan SONRA cagrilir (kasa.bin yolu veri dizinindedir). */
int         umay_gomulu_kasa_var(void);
int         umay_gomulu_kasa_acik(void);
int         umay_gomulu_kasa_kur(const char *parola);
int         umay_gomulu_kasa_ac(const char *parola);
void        umay_gomulu_kasa_kilitle(void);
int         umay_gomulu_kasa_say(void);
int         umay_gomulu_kasa_bilgi(int i, char *ad, uint32_t n, int *tur, uint32_t *boy);
int         umay_gomulu_kasa_yaz(const char *ad, int tur, const void *veri, uint32_t n);
int32_t     umay_gomulu_kasa_boy(const char *ad);
int32_t     umay_gomulu_kasa_oku(const char *ad, void *cik, uint32_t n);
int         umay_gomulu_kasa_sil(const char *ad);
int         umay_gomulu_kasa_parola_degistir(const char *eski, const char *yeni);
/* Biyometrik kisayol: anahtari uygulama isletim sisteminin biyometrik korumali deposuna koyar. */
int         umay_gomulu_kasa_anahtar(uint8_t cik[32]);
int         umay_gomulu_kasa_ac_anahtarla(const uint8_t anahtar[32]);

void        umay_gomulu_kapat(void);
const char *umay_gomulu_surum(void);

#endif

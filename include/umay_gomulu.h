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

void        umay_gomulu_kapat(void);
const char *umay_gomulu_surum(void);

#endif

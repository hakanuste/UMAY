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

void        umay_gomulu_kapat(void);
const char *umay_gomulu_surum(void);

#endif

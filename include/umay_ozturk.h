/* Öz Türkçe süzgeci: yanıtta yabancı kökenli sözcük bulur, Türkçe karşılığını önerir.
 * Sözcük listesi gömülü + <kut>/ozturkce.txt ("yabancı=karşılık", # yorum). */
#ifndef UMAY_OZTURK_H
#define UMAY_OZTURK_H
#include "umay.h"

typedef struct { char sozcuk[48]; char oneri[64]; } ozturk_bulgu_t;

/* Ek liste dosyasını yükler (yoksa sessizce yalnız gömülü liste). Yüklenen ek satır sayısını döner. */
int ozturk_yukle(const char *kut);
/* Metindeki (benzersiz) yabancı sözcükleri bulur; bulgu sayısını döner. Ek/çekim alır (tefekkürün). */
int ozturk_tara(const char *metin, ozturk_bulgu_t *b, int en_cok);
/* "tefekkür (yerine: düşünce), ..." biçiminde yasak listesi yazar. */
void ozturk_liste(const ozturk_bulgu_t *b, int adet, char *cikti, uint32_t n);
#endif

/* =====================================================================
 * ÇEVİRİ — ÇEVİRMEN ajanının çekirdeği.
 *
 * Çeviri yöntemleri (sırayla denenir):
 *   1. MOTOR   gömülü model takılıysa (ceviri_motor_ata) onunla.
 *   2. AĞ      web çeviri hizmeti (MyMemory; UMAY_AG_CEVIRI ile değişir),
 *              yalnız öğrenme açıkken ve ağ varken. Cümle sınırında ≤450
 *              baytlık parçalar halinde.
 *   3. SÖZLÜK  kelime kelime; <kütüphane>/sozluk-ceviri.txt ("en=tr" satırları)
 *              + küçük dahili çekirdek. Çevrilen sözcük oranı düşükse
 *              çeviri YAPILMAZ (kötü çeviri sunmak yerine "çeviremedim").
 *
 * Dürüstlük: çıktı hangi yöntemle üretildiğini söyler; makine çevirisi
 * "doğrulanmamış"tır. Kütüphaneye yalnız MOTOR/AĞ çevirileri yazılır
 * ("ceviri-" önekli, KT_DIS, #ozgun_dil künyeli); sözlük çevirisi yazılmaz.
 * ===================================================================== */
#ifndef UMAY_CEVIRI_H
#define UMAY_CEVIRI_H
#include "umay.h"

typedef enum { CY_YOK = 0, CY_MOTOR, CY_AG, CY_SOZLUK } ceviri_yontem_t;

typedef int (*ceviri_motor_t)(const char *kd, const char *hd, const char *metin, char *cik, uint32_t n);
void ceviri_motor_ata(ceviri_motor_t f);               /* 0 = motor yok */

/* metni kd → hd çevirir. Dönüş: yazılan bayt (>0) ya da UM_YOK (çevrilemedi).
 * yontem (boş olabilir): hangi yolla yapıldı. */
int  ceviri_yap(const char *kd, const char *hd, const char *metin, char *cik, uint32_t n, ceviri_yontem_t *yontem);
const char *ceviri_yontem_adi(ceviri_yontem_t y);

/* "ingilizceye çevir: ..." gibi bir isteği çözer. kd boş = otomatik kestir. */
int  ceviri_istek_coz(const char *soz, char *kd, char *hd, char *metin, uint32_t n);
/* "ingilizce" / "ingilizceye" / "almanca" ... → "en" / "de" ... ; yoksa UM_YOK */
int  ceviri_dil_kodu(const char *sozcuk, char *kod);

/* Yabancı dildeki bir kütüphane belgesini Türkçe çeviri için sıraya alır. */
int  ceviri_kayit(const char *yol);
int  ceviri_bekleyen(void);
/* Sırayı işler: en çok en_cok belge. Dönüş: yazılan çeviri belgesi. */
int  ceviri_gecis(int en_cok);
void ceviri_bosta(void);
#endif

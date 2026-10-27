/* =====================================================================
 * ARAŞTIR — planlı, sınırlı bütçeli, kaynaklı araştırma.
 *
 * Bir soruyu şöyle ele alır:
 *   1. PLAN     soruyu açılara böler: bütün anahtar sözcükler birlikte,
 *               sonra en ayırt edici tek tek sözcükler.
 *   2. ARAMA    her açıyı kütüphanede arar (bütçe: en çok 5 arama).
 *               Her çağrıdan önce BEKÇİ'ye sorulur (döngü engeli).
 *   3. GENİŞLET kapsam yetmezse ve web açıksa konuyu web'den öğrenir,
 *               sonra yeniden arar.
 *   4. ÇAPRAZ   her anahtar sözcüğün kaç AYRI kaynakta geçtiğine bakar;
 *               çelişen kaynakları işaretler.
 *   5. RAPOR    numaralı kaynaklarla yazar. YALNIZ bulunanı söyler:
 *               rapordaki her cümle bir kaynaktan aynen alınmıştır.
 *
 * Model çağrılmaz; rapor belirlenimlidir ve her çalıştırmada aynıdır.
 * Tamsayı hesap; ÖTÜKEN'de de derlenir.
 * ===================================================================== */
#ifndef UMAY_ARASTIR_H
#define UMAY_ARASTIR_H
#include "umay.h"
#include "umay_bilgi.h"
#include "umay_us.h"

#define ARASTIR_ACI_MAX    4
#define ARASTIR_KAYNAK_MAX 6
#define ARASTIR_BUTCE      5      /* en çok arama/ağ çağrısı */
#define ARASTIR_TERIM_MAX  6

typedef struct {
    char        soru[256];
    char        aci[ARASTIR_ACI_MAX][160];
    int         aci_sayi;
    baglam_t    kaynak[ARASTIR_KAYNAK_MAX];   /* tekilleştirilmiş, [1]..[n] */
    int         kaynak_sayi;

    char        terim[ARASTIR_TERIM_MAX][24];
    int         terim_sayi;
    int         terim_kaynak[ARASTIR_TERIM_MAX]; /* terimi içeren AYRI kaynak sayısı */
    int         kapsam;                          /* 0..100 */
    int         arama;                           /* yapılan arama sayısı */
    int         web_parca;                       /* bu araştırmada web'den öğrenilen */
    int         bekci_uyari, bekci_engel;
    char        bekci_not[96];
    us_guven_t  guven;
    char        guven_neden[128];
    int         catisma;
    char        catisma_notu[192];
} arastirma_t;

/* web != 0: kapsam yetmezse ogren_hemen ile web'den kapatmayı dener.
 * kategori: web belgesinin yazılacağı alan (bilinmiyorsa "ortak"). */
int  arastir_yurut(const char *soru, const char *kategori, int web, arastirma_t *r);

/* Yalnız kütüphanede, web'siz: soru kaç % karşılanıyor? (eğitim ölçüsü) */
int  arastir_kapsam(const char *soru);

uint32_t arastir_rapor(const arastirma_t *r, char *c, uint32_t n);
#endif

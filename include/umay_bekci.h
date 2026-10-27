/* =====================================================================
 * BEKÇİ — ajan döngü bekçisi.
 *
 * Bir ajanın araç çağrıları (arama, ağ ...) kısır döngüye girebilir:
 * aynı çağrıyı durmadan yinelemek, iki çağrı arasında gidip gelmek,
 * bir araca bütçesinden fazla yüklenmek. Bekçi her çağrıdan ÖNCE
 * sorulur ve çağrıya izin verir, uyarır ya da engeller.
 *
 *   - Aynı (araç, bağımsız değişken) çağrısı en çok max_ayni kez.
 *   - Araç başına bütçe.
 *   - İkili/üçlü gidip gelme (A-B-A-B-A-B, A-B-C-A-B-C).
 *   - UYAR-SONRA-ENGELLE: bir döngü türü ilk kez görülünce yalnız
 *     uyarılır (ajan yön değiştirebilsin), yinelenirse engellenir.
 *
 * Tamsayı ve sabit bellek; ÖTÜKEN'de de derlenir.
 * ===================================================================== */
#ifndef UMAY_BEKCI_H
#define UMAY_BEKCI_H
#include "umay.h"

#define BEKCI_HASH_MAX 48
#define BEKCI_ARAC_MAX 8
#define BEKCI_DIZI_MAX 12
#define BEKCI_UYARI_MAX 8

typedef struct {
    int max_ayni;               /* aynı çağrı üst sınırı (varsayılan 3)   */
    int pencere;                /* gidip gelme penceresi (varsayılan 6)   */
    int arac_butce;             /* araç başına çağrı üst sınırı (5)       */

    uint32_t hash[BEKCI_HASH_MAX];
    int      say[BEKCI_HASH_MAX];
    int      n;
    uint32_t arac[BEKCI_ARAC_MAX];
    int      arac_say[BEKCI_ARAC_MAX];
    int      arac_n;
    uint32_t dizi[BEKCI_DIZI_MAX];
    int      dizi_n;
    uint32_t uyarilan[BEKCI_UYARI_MAX];
    int      uyarilan_n;
} bekci_t;

typedef struct {
    int  engel;                 /* 1 = bu çağrı YAPILMAMALI */
    int  uyari;                 /* 1 = yapılabilir ama döngü kokusu var */
    char neden[96];
} bekci_karar_t;

void          bekci_kur(bekci_t *b);
bekci_karar_t bekci_sor(bekci_t *b, const char *arac, const char *arguman);
#endif

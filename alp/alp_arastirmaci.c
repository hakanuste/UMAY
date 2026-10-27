/* ARAŞTIRMACI — planlı araştırma ajanı.
 *
 * ARAŞTIR görevlerini alır: soruyu açılara böler, bütçeli ve bekçi
 * denetimli arar, gerekirse web'den öğrenir, çapraz denetler ve
 * kaynaklı rapor yazar (core/umay_arastir.c). Model çağırmaz; rapordaki
 * her cümle bir kaynaktan aynen alınmıştır. */
#include "umay_alp.h"
#include "umay_arastir.h"
#include "umay_egit.h"
#include "umay_port.h"
#include "umay_str.h"

typedef struct {
    int     adim_no;
    gorev_t gorev;
    char    rapor[GOREV_SONUC_MAX];
} arastirmaci_ic_t;

static arastirmaci_ic_t g_ic;

static int uygun(const gorev_t *g) { return g->tur == GT_ARASTIR; }

static int basla(alp_t *a, const gorev_t *g) {
    um_bellek_doldur(&g_ic, 0, sizeof g_ic);
    um_bellek_kopya(&g_ic.gorev, g, sizeof *g);
    a->ic = &g_ic;
    return UM_OLDU;
}

static alp_durum_t adim(alp_t *a) {
    arastirmaci_ic_t *ic = (arastirmaci_ic_t *)a->ic;
    if (ic->adim_no++ == 0) return ALP_KOSUYOR;     /* planı göster, sonra koş */

    arastirma_t *r = (arastirma_t *)up_ayir(sizeof *r);
    if (!r) { um_kopya(ic->rapor, sizeof ic->rapor, "Araştırma için bellek ayıramadım."); return ALP_YANLIS; }
    arastir_yurut(ic->gorev.istem, "ortak", 1, r);
    arastir_rapor(r, ic->rapor, sizeof ic->rapor);
    if (r->catisma && a->gorev) {
        a->gorev->catisma = 1;
        um_kopya(a->gorev->catisma_notu, sizeof a->gorev->catisma_notu, r->catisma_notu);
    }
    karne_yaz("ARAŞTIRMACI", ic->gorev.istem, (int)r->guven, r->kaynak_sayi);
    up_biraktir(r);
    return ALP_BITTI;
}

static void topla(alp_t *a, sonuc_t *s) {
    arastirmaci_ic_t *ic = (arastirmaci_ic_t *)a->ic;
    um_kopya(s->oz, sizeof s->oz, ic->rapor);
}

static alp_t g_arastirmaci = {
    .ad = "ARAŞTIRMACI", .is = "planlı, kaynaklı araştırma",
    .alan = ALAN_YOK,
    .tur_maskesi = GT_BIT(GT_ARASTIR),
    .kategori = { 0 },
    .yetki_gerek = 0,
    .uygun_mu = uygun, .basla = basla, .adim = adim, .topla = topla,
    .cikti_al = 0,
    .durum = ALP_BOS
};
alp_t *alp_arastirmaci(void) { return &g_arastirmaci; }

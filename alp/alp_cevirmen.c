/* ÇEVİRMEN — dil çevirisi ajanı.
 *
 * "ingilizceye çevir: ...", "şunu türkçeye çevir ...", "... ingilizcesi nedir"
 * gibi istekleri alır. Yöntemi (gömülü model / web çeviri / sözlük) çıktıda
 * açıkça yazar; makine çevirisini "doğrulanmamış" diye işaretler. Çeviremiyorsa
 * uydurmaz, söyler. Bkz. core/umay_ceviri.c */
#include "umay_alp.h"
#include "umay_ceviri.h"
#include "umay_egit.h"
#include "umay_port.h"
#include "umay_str.h"

typedef struct {
    int     adim_no;
    gorev_t gorev;
    char    cevap[GOREV_SONUC_MAX];
} cevirmen_ic_t;

static cevirmen_ic_t g_ic;

static int uygun(const gorev_t *g) { return g->tur == GT_CEVIR; }

static int basla(alp_t *a, const gorev_t *g) {
    um_bellek_doldur(&g_ic, 0, sizeof g_ic);
    um_bellek_kopya(&g_ic.gorev, g, sizeof *g);
    a->ic = &g_ic;
    return UM_OLDU;
}

static alp_durum_t adim(alp_t *a) {
    cevirmen_ic_t *ic = (cevirmen_ic_t *)a->ic;
    if (ic->adim_no++ == 0) return ALP_KOSUYOR;
    char kd[8], hd[8];
    char *metin = (char *)up_ayir(GOREV_SONUC_MAX);
    char *cik = (char *)up_ayir(GOREV_SONUC_MAX);
    if (!metin || !cik) {
        um_kopya(ic->cevap, sizeof ic->cevap, "Çeviri için bellek ayıramadım.");
        if (metin) up_biraktir(metin);
        if (cik) up_biraktir(cik);
        return ALP_YANLIS;
    }
    int guven = 0;
    if (ceviri_istek_coz(ic->gorev.istem, kd, hd, metin, GOREV_SONUC_MAX) != UM_OLDU) {
        um_kopya(ic->cevap, sizeof ic->cevap,
                 "Neyi çevireyim? Şöyle yaz: ingilizceye çevir: <metin>   ya da   türkçeye çevir: <metin>");
    } else {
        ceviri_yontem_t y;
        int r = ceviri_yap(kd, hd, metin, cik, GOREV_SONUC_MAX, &y);
        if (r > 0) {
            um_bicim(ic->cevap, sizeof ic->cevap, "%s→%s (%s):\n%s\n", kd, hd, ceviri_yontem_adi(y), cik);
            if (y == CY_SOZLUK)
                um_ekle(ic->cevap, sizeof ic->cevap, "Not: kelime kelime sözlük çevirisidir; dil bilgisi gözetilmez. Düzgün çeviri için web çeviri ya da gömülü model gerekir.\n");
            else
                um_ekle(ic->cevap, sizeof ic->cevap, "Not: makine çevirisidir, doğrulanmamıştır.\n");
            guven = y == CY_SOZLUK ? 1 : 2;
        } else {
            um_bicim(ic->cevap, sizeof ic->cevap,
                     "Bunu %s→%s çeviremedim: web çeviri hizmetine ulaşılamadı (ya da kapalı), gömülü model yok ve sözlüğüm bu metni yeterince kapsamıyor. "
                     "Uydurma çeviri sunmuyorum. Ağ açıkken yeniden dene; ya da sözlük-ceviri.txt dosyasına 'sözcük=karşılık' satırları ekleyebilirsin.\n", kd, hd);
        }
    }
    karne_yaz("ÇEVİRMEN", ic->gorev.istem, guven, guven > 0 ? 1 : 0);
    up_biraktir(metin); up_biraktir(cik);
    return ALP_BITTI;
}

static void topla(alp_t *a, sonuc_t *s) {
    cevirmen_ic_t *ic = (cevirmen_ic_t *)a->ic;
    um_kopya(s->oz, sizeof s->oz, ic->cevap);
}

static alp_t g_cevirmen = {
    .ad = "ÇEVİRMEN", .is = "dil çevirisi",
    .alan = ALAN_YOK,
    .tur_maskesi = GT_BIT(GT_CEVIR),
    .kategori = { 0 },
    .yetki_gerek = 0,
    .uygun_mu = uygun, .basla = basla, .adim = adim, .topla = topla,
    .cikti_al = 0,
    .durum = ALP_BOS
};
alp_t *alp_cevirmen(void) { return &g_cevirmen; }

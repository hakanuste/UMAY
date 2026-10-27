#include "umay_bekci.h"
#include "umay_str.h"

static uint32_t fnv(const char *a, const char *b) {
    uint32_t h = 2166136261u;
    for (; a && *a; a++) { h ^= (uint8_t)*a; h *= 16777619u; }
    h ^= 0x1F; h *= 16777619u;
    for (; b && *b; b++) { h ^= (uint8_t)*b; h *= 16777619u; }
    return h;
}

void bekci_kur(bekci_t *b) {
    um_bellek_doldur(b, 0, sizeof *b);
    b->max_ayni = 3;
    b->pencere = 6;
    b->arac_butce = 5;
}

/* Döngü türü ilk kez mi görülüyor? İlkse uyarı olarak kaydeder. */
static int ilk_uyari(bekci_t *b, uint32_t anahtar) {
    for (int i = 0; i < b->uyarilan_n; i++) if (b->uyarilan[i] == anahtar) return 0;
    if (b->uyarilan_n < BEKCI_UYARI_MAX) b->uyarilan[b->uyarilan_n++] = anahtar;
    return 1;
}

static bekci_karar_t karar(bekci_t *b, uint32_t anahtar, const char *neden) {
    bekci_karar_t k;
    um_bellek_doldur(&k, 0, sizeof k);
    um_kopya(k.neden, sizeof k.neden, neden);
    if (ilk_uyari(b, anahtar)) k.uyari = 1; else k.engel = 1;
    return k;
}

/* son pencere'de dönem p ile yinelenen desen var mı (p=2: ABABAB, p=3: ABCABC) */
static int gidip_gelme(const bekci_t *b, int p) {
    int w = b->pencere;
    if (w < 2 * p || b->dizi_n < w) return 0;
    const uint32_t *s = b->dizi + (b->dizi_n - w);
    for (int i = p; i < w; i++) if (s[i] != s[i - p]) return 0;
    for (int i = 1; i < p; i++) if (s[i] == s[0]) return 0;   /* tek araç = ping-pong değil */
    return 1;
}

bekci_karar_t bekci_sor(bekci_t *b, const char *arac, const char *arguman) {
    bekci_karar_t ok;
    um_bellek_doldur(&ok, 0, sizeof ok);

    uint32_t h = fnv(arac, arguman), ah = fnv(arac, "");
    /* 1. aynı çağrı */
    int idx = -1;
    for (int i = 0; i < b->n; i++) if (b->hash[i] == h) { idx = i; break; }
    if (idx < 0 && b->n < BEKCI_HASH_MAX) { idx = b->n++; b->hash[idx] = h; b->say[idx] = 0; }
    if (idx >= 0 && ++b->say[idx] > b->max_ayni) {
        char s[96];
        um_bicim(s, sizeof s, "'%s' aynı bağımsız değişkenle %d kez çağrıldı", arac, b->say[idx]);
        return karar(b, h ^ 1u, s);
    }
    /* 2. araç bütçesi */
    int ai = -1;
    for (int i = 0; i < b->arac_n; i++) if (b->arac[i] == ah) { ai = i; break; }
    if (ai < 0 && b->arac_n < BEKCI_ARAC_MAX) { ai = b->arac_n++; b->arac[ai] = ah; b->arac_say[ai] = 0; }
    if (ai >= 0 && ++b->arac_say[ai] > b->arac_butce) {
        char s[96];
        um_bicim(s, sizeof s, "'%s' aracı bütçesini (%d) aştı", arac, b->arac_butce);
        return karar(b, ah ^ 2u, s);
    }
    /* 3. gidip gelme */
    if (b->dizi_n == BEKCI_DIZI_MAX) {
        for (int i = 1; i < BEKCI_DIZI_MAX; i++) b->dizi[i - 1] = b->dizi[i];
        b->dizi_n--;
    }
    b->dizi[b->dizi_n++] = ah;
    if (gidip_gelme(b, 2) || gidip_gelme(b, 3))
        return karar(b, 3u, "araçlar arasında gidip gelme (döngü) görüldü");
    return ok;
}

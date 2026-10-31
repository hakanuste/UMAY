/* ZAMAN — bkz. umay_zaman.h */
#include "umay_zaman.h"
#include "umay_str.h"

static const struct { const char *kucuk, *buyuk; int gun; } GUNLER[] = {
    {"pazartesi", "Pazartesi", 1}, {"cumartesi", "Cumartesi", 6}, {"cuma", "Cuma", 5},
    {"salı", "Salı", 2}, {"çarşamba", "Çarşamba", 3}, {"perşembe", "Perşembe", 4},
    {"pazar", "Pazar", 7}, {0, 0, 0}
};

/* metinden bir parçayı (ilk görüldüğü yerden) siler; ardından "günü"/"günü," gibi ek de gider */
static void sil(char *m, const char *parca) {
    char *p = (char *)um_icinde(m, parca);
    if (!p) return;
    uint32_t n = um_uzun(parca);
    char *e = p + n;
    if (um_icinde(e, " günü") == e) e += 5;
    um_bellek_kopya(p, e, um_uzun(e) + 1);
}

static int sayi2(const char *p, int *deger) {
    int n = 0, v = 0;
    while (n < 2 && p[n] >= '0' && p[n] <= '9') { v = v * 10 + (p[n] - '0'); n++; }
    if (!n) return 0;
    *deger = v;
    return n;
}

int zaman_ayikla(const char *metin, zaman_t *z) {
    um_bellek_doldur(z, 0, sizeof *z);
    z->gun_ofset = -1; z->saat = -1;
    if (!metin || !*metin) return 0;
    char m[200], k[200];
    um_kopya(m, sizeof m, metin);
    um_kucult(k, sizeof k, metin);

    /* gün adı */
    for (int i = 0; GUNLER[i].kucuk; i++) {
        if (um_icinde(k, GUNLER[i].kucuk)) {
            z->haftagunu = GUNLER[i].gun; z->var = 1;
            sil(m, GUNLER[i].buyuk); sil(m, GUNLER[i].kucuk);
            break;
        }
    }
    /* göreli gün */
    if (um_icinde(k, "öbür gün") || um_icinde(k, "ertesi gün")) {
        z->gun_ofset = 2; z->var = 1; sil(m, "Öbür gün"); sil(m, "öbür gün"); sil(m, "Ertesi gün"); sil(m, "ertesi gün");
    } else if (um_icinde(k, "yarın")) {
        z->gun_ofset = 1; z->var = 1; sil(m, "Yarın"); sil(m, "yarın");
    } else if (um_icinde(k, "bugün")) {
        z->gun_ofset = 0; z->var = 1; sil(m, "Bugün"); sil(m, "bugün");
    }
    /* saat: SS:DD ya da SS.DD ya da "saat SS" */
    int saat = -1, dk = 0;
    for (const char *p = k; *p && saat < 0; p++) {
        if (*p < '0' || *p > '9' || (p > k && p[-1] >= '0' && p[-1] <= '9')) continue;
        int s, d, n1 = sayi2(p, &s);
        if (n1 && (p[n1] == ':' || p[n1] == '.') && sayi2(p + n1 + 1, &d) == 2 && s < 24 && d < 60) { saat = s; dk = d; }
    }
    const char *sa = um_icinde(k, "saat ");
    if (saat < 0 && sa) { int s; if (sayi2(sa + 5, &s) && s < 24) { saat = s; dk = 0; } }
    if (saat >= 0) {
        z->var = 1;
        if (saat < 12 && (um_icinde(k, "akşam") || um_icinde(k, "öğleden sonra") || um_icinde(k, "gece"))) saat += 12;
        z->saat = saat; z->dakika = dk;
        /* zaman sözcüklerini işten çıkar: "saat 14:30", "14:30", "saat 10" */
        char *q = (char *)um_icinde(m, "saat ");
        if (q) { uint32_t n = 5; while (q[n] >= '0' && q[n] <= '9') n++; if (q[n] == ':' || q[n] == '.') { n++; while (q[n] >= '0' && q[n] <= '9') n++; }
                 if (q[n] == '\'' ) { n++; while (q[n] && q[n] != ' ') n++; }   /* saat 10'da */
                 um_bellek_kopya(q, q + n, um_uzun(q + n) + 1); }
        else { for (char *p = m; *p; p++) if (*p >= '0' && *p <= '9' && (p == m || p[-1] < '0' || p[-1] > '9')) {
                   uint32_t n = 0; while (p[n] >= '0' && p[n] <= '9') n++;
                   if (p[n] == ':' || p[n] == '.') { n++; while (p[n] >= '0' && p[n] <= '9') n++; um_bellek_kopya(p, p + n, um_uzun(p + n) + 1); break; } } }
        sil(m, "akşam"); sil(m, "Akşam"); sil(m, "sabah"); sil(m, "Sabah"); sil(m, "öğleden sonra");
    }
    if (!z->var) return 0;
    /* işi düzenle: baştaki/sondaki ayraç ve boşluklar */
    char *b = m;
    while (*b == ' ' || *b == ',' || *b == ':' || *b == '-') b++;
    uint32_t u = um_uzun(b);
    while (u && (b[u - 1] == ' ' || b[u - 1] == ',' || b[u - 1] == '.')) b[--u] = 0;
    um_kopya(z->is, sizeof z->is, b);
    return 1;
}

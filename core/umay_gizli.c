/* GİZLİLİK DUVARI — bkz. umay_gizli.h */
#include "umay_gizli.h"
#include "umay_str.h"
#include "umay_port.h"
#include "umay_ortak.h"

static const char *GOMULU[] = {
    "şifre", "parola", "iban", "tapu", "parsel", "maaş", "miras", "hisseli", 0
};

#define GZ_MAX 512
static char g_kul[GZ_MAX][48];
static int  g_kul_n;
static char g_yuklu[200];
static int  g_yuklendi;

int gizli_yukle(const char *kut) {
    g_kul_n = 0;
    g_yuklendi = 1;
    um_kopya(g_yuklu, sizeof g_yuklu, kut ? kut : "");
    if (!kut) return 0;
    char yol[320];
    um_bicim(yol, sizeof yol, "%s/gizli.txt", kut);
    int64_t boy = up_boy(yol);
    if (boy <= 0 || boy > 131072) return 0;
    char *m = (char *)up_ayir((uint64_t)boy + 1);
    if (!m) return 0;
    up_tut_t t = up_ac(yol, 0);
    int64_t o = t == UP_GECERSIZ ? -1 : up_oku(t, m, (uint64_t)boy);
    if (t != UP_GECERSIZ) up_kapat(t);
    if (o <= 0) { up_biraktir(m); return 0; }
    m[o] = 0;
    char *p = m;
    while (*p && g_kul_n < GZ_MAX) {
        char *s = p;
        while (*p && *p != '\n') p++;
        if (*p) *p++ = 0;
        while (*s == ' ' || *s == '\t') s++;
        uint32_t u = um_uzun(s);
        while (u && (s[u - 1] == ' ' || s[u - 1] == '\r' || s[u - 1] == '\t')) s[--u] = 0;
        if (!*s || *s == '#' || u < 3) continue;       /* 3 harften kısa sözcük çok yanlış eşleşir */
        um_kucult(g_kul[g_kul_n], 48, s);
        g_kul_n++;
    }
    up_biraktir(m);
    return g_kul_n;
}

static void liste_hazir(void) {
    const char *kok = ayar_metin("kutuphane", "kut");
    if (!g_yuklendi || !um_esit(g_yuklu, kok)) gizli_yukle(kok);
}

int gizli_mi(const char *metin) {
    if (!metin || !*metin) return 0;
    liste_hazir();
    static char k[2048];
    um_kucult(k, sizeof k, metin);      /* İ->i, I->ı, Ş->ş ... boy değişse de doğru küçültür */
    for (int i = 0; GOMULU[i]; i++) if (um_icinde(k, GOMULU[i])) return 1;
    for (int i = 0; i < g_kul_n; i++) if (um_icinde(k, g_kul[i])) return 1;
    return 0;
}

static int onaltilik(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int gizli_adres_mi(const char *url) {
    if (!url) return 0;
    static char c[2048];
    uint32_t o = 0;
    for (const char *p = url; *p && o + 1 < sizeof c; p++) {
        if (*p == '%' && onaltilik(p[1]) >= 0 && onaltilik(p[2]) >= 0) {
            c[o++] = (char)(onaltilik(p[1]) * 16 + onaltilik(p[2])); p += 2;
        } else c[o++] = (*p == '+') ? ' ' : *p;
    }
    c[o] = 0;
    return gizli_mi(c);
}

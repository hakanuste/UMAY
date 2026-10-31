/* GİZLİLİK DUVARI — bkz. umay_gizli.h */
#include "umay_gizli.h"
#include "umay_str.h"
#include "umay_port.h"
#include "umay_ortak.h"

/* Harfleri ASCII'ye katlar ve küçültür: ç->c ğ->g ı/İ->i ö->o ş->s ü->u, A-Z -> a-z.
 * Böylece "Koçarlı", "KOCARLI" ve "kocarli" aynı sayılır (Türkçe klavyesiz yazılanlar da yakalanır). */
static uint32_t katla(char *hedef, uint32_t n, const char *kaynak) {
    uint32_t o = 0;
    const unsigned char *p = (const unsigned char *)kaynak;
    while (*p && o + 1 < n) {
        unsigned char c = *p;
        if (c == 0xC3 && p[1]) {
            unsigned char d = p[1]; char r = 0;
            if (d == 0xA7 || d == 0x87) r = 'c';
            else if (d == 0xB6 || d == 0x96) r = 'o';
            else if (d == 0xBC || d == 0x9C) r = 'u';
            else if (d == 0xA2 || d == 0x82) r = 'a';
            else if (d == 0xAE || d == 0x8E) r = 'i';
            else if (d == 0xBB || d == 0x9B) r = 'u';
            if (r) { hedef[o++] = r; p += 2; continue; }
        } else if (c == 0xC4 && p[1]) {
            unsigned char d = p[1]; char r = 0;
            if (d == 0x9F || d == 0x9E) r = 'g';
            else if (d == 0xB1 || d == 0xB0) r = 'i';
            if (r) { hedef[o++] = r; p += 2; continue; }
        } else if (c == 0xC5 && p[1]) {
            unsigned char d = p[1];
            if (d == 0x9F || d == 0x9E) { hedef[o++] = 's'; p += 2; continue; }
        }
        hedef[o++] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : (char)c;
        p++;
    }
    hedef[o] = 0;
    return o;
}

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
        katla(g_kul[g_kul_n], 48, s);
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
    static char k[2048], g[48];
    katla(k, sizeof k, metin);
    for (int i = 0; GOMULU[i]; i++) { katla(g, sizeof g, GOMULU[i]); if (um_icinde(k, g)) return 1; }
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

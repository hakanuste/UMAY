#include "umay_ozturk.h"
#include "umay_str.h"
#include "umay_port.h"

typedef struct { const char *kok; const char *oneri; } sozluk_t;

/* ÖNERİ listesi: yaygın, yanlış anlaşılmayan karşılıklar. Divanü Lügati't-Türk kaynaklı
 * doğrulama bekleyen girdiler için <kut>/ozturkce.txt eklenebilir. */
static const sozluk_t GOMULU[] = {
    {"tefekkür", "düşünce, düşünme"}, {"fikir", "düşünce"}, {"ilim", "bilim, bilgi"},
    {"hayat", "yaşam"}, {"sebep", "neden"}, {"netice", "sonuç"}, {"mesele", "sorun, konu"},
    {"mevzu", "konu"}, {"hadise", "olay"}, {"vaziyet", "durum"}, {"muhafaza", "koruma"},
    {"istikamet", "yön"}, {"tabiat", "doğa"}, {"tabii", "doğal"}, {"kuvvet", "güç"},
    {"kudret", "güç"}, {"hakikat", "gerçek"}, {"hakiki", "gerçek"}, {"imkan", "olanak"},
    {"imkân", "olanak"}, {"ihtiyaç", "gereksinim"}, {"lazım", "gerek"}, {"muhakkak", "kesin"},
    {"umumiyetle", "genellikle"}, {"ekseriyetle", "çoğunlukla"}, {"hususi", "özel"},
    {"husus", "konu"}, {"cevap", "yanıt"}, {"sual", "soru"}, {"cihaz", "aygıt"},
    {"tahlil", "çözümleme"}, {"tecrübe", "deneyim"}, {"kıymet", "değer"}, {"fayda", "yarar"},
    {"mühim", "önemli"}, {"ehemmiyet", "önem"}, {"vazife", "görev"}, {"memleket", "ülke"},
    {"vatan", "yurt"}, {"millet", "ulus"}, {"cemiyet", "toplum"}, {"ictimai", "toplumsal"},
    {"hürriyet", "özgürlük"}, {"cesaret", "yüreklilik"}, {"akıl", "us, düşünce"},
    {"zihin", "us, kafa"}, {"doktrin", "öğreti"}, {"teori", "kuram"}, {"metot", "yöntem"},
    {"sistem", "dizge"}, {"proje", "tasarı"}, {"analiz", "çözümleme"}, {"sentez", "bireşim"},
    {"mekanizma", "düzenek"}, {"prensip", "ilke"}, {"ilke", ""}
};
#define GOMULU_N ((int)(sizeof GOMULU / sizeof GOMULU[0]) - 1)  /* son girdi sınırlayıcı */

#define EK_MAX 256
static char g_ek_kok[EK_MAX][48], g_ek_oneri[EK_MAX][64];
static int g_ek_n;

int ozturk_yukle(const char *kut) {
    g_ek_n = 0;
    if (!kut) return 0;
    char yol[320];
    um_bicim(yol, sizeof yol, "%s/ozturkce.txt", kut);
    int64_t boy = up_boy(yol);
    if (boy <= 0 || boy > 262144) return 0;
    char *m = (char *)up_ayir((uint64_t)boy + 1);
    if (!m) return 0;
    up_tut_t t = up_ac(yol, 0);
    int64_t o = t == UP_GECERSIZ ? -1 : up_oku(t, m, (uint64_t)boy);
    if (t != UP_GECERSIZ) up_kapat(t);
    if (o <= 0) { up_biraktir(m); return 0; }
    m[o] = 0;
    char *p = m;
    while (*p && g_ek_n < EK_MAX) {
        char *s = p;
        while (*p && *p != '\n') p++;
        if (*p) *p++ = 0;
        while (*s == ' ' || *s == '\t') s++;
        if (*s == '#' || !*s) continue;
        char *e = s;
        while (*e && *e != '=') e++;
        if (!*e) continue;
        *e++ = 0;
        uint32_t u = um_uzun(s);
        while (u && (s[u - 1] == ' ' || s[u - 1] == '\r')) s[--u] = 0;
        while (*e == ' ') e++;
        u = um_uzun(e);
        while (u && (e[u - 1] == ' ' || e[u - 1] == '\r')) e[--u] = 0;
        if (!*s || !*e) continue;
        char kc[48];
        um_kucult(kc, sizeof kc, s);
        um_kopya(g_ek_kok[g_ek_n], 48, kc);
        um_kopya(g_ek_oneri[g_ek_n], 64, e);
        g_ek_n++;
    }
    up_biraktir(m);
    return g_ek_n;
}

/* Sözcük kökle başlıyor ve kalan kısa bir ek olabilir (en çok 6 bayt) mi? */
static int kok_eslesir(const char *soz, const char *kok) {
    uint32_t k = um_uzun(kok), s = um_uzun(soz);
    if (k < 4 || s < k || s - k > 6) return 0;
    return um_esit_n(soz, kok, k);
}

static int bulundu_mu(const ozturk_bulgu_t *b, int n, const char *soz) {
    for (int i = 0; i < n; i++) if (um_esit(b[i].sozcuk, soz)) return 1;
    return 0;
}

int ozturk_tara(const char *metin, ozturk_bulgu_t *b, int en_cok) {
    if (!metin || !b || en_cok <= 0) return 0;
    int n = 0;
    const char *p = metin;
    char soz[96], kc[192];
    while (*p && n < en_cok) {
        uint32_t bayt = 1, kn = um_utf8_oku(p, &bayt);
        if (!um_kelime_karakteri(kn)) { p += bayt ? bayt : 1; continue; }
        uint32_t u = 0;
        while (*p) {
            kn = um_utf8_oku(p, &bayt);
            if (!um_kelime_karakteri(kn)) break;
            if (u + bayt < sizeof soz) { um_bellek_kopya(soz + u, p, bayt); u += bayt; }
            p += bayt ? bayt : 1;
        }
        soz[u] = 0;
        um_kucult(kc, sizeof kc, soz);
        const char *oneri = 0, *kok = 0;
        for (int i = 0; i < GOMULU_N && !oneri; i++)
            if (kok_eslesir(kc, GOMULU[i].kok)) { oneri = GOMULU[i].oneri; kok = GOMULU[i].kok; }
        for (int i = 0; i < g_ek_n && !oneri; i++)
            if (kok_eslesir(kc, g_ek_kok[i])) { oneri = g_ek_oneri[i]; kok = g_ek_kok[i]; }
        if (!oneri || bulundu_mu(b, n, kok)) continue;
        um_kopya(b[n].sozcuk, sizeof b[n].sozcuk, kok);
        um_kopya(b[n].oneri, sizeof b[n].oneri, oneri);
        n++;
    }
    return n;
}

void ozturk_liste(const ozturk_bulgu_t *b, int adet, char *cikti, uint32_t n) {
    if (!cikti || !n) return;
    cikti[0] = 0;
    for (int i = 0; i < adet; i++) {
        if (i) um_ekle(cikti, n, "; ");
        um_ekle(cikti, n, b[i].sozcuk);
        um_ekle(cikti, n, " (yerine: ");
        um_ekle(cikti, n, b[i].oneri);
        um_ekle(cikti, n, ")");
    }
}

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

/* ---- öğrenme ---- */
static int salt_sozcuk(const char *a, const char *b, char *c, uint32_t n) {
    /* [a,b) tek bir sözcük mü (harf; tire/kesme yok)? Küçültülmüş hâlini c'ye yazar. */
    while (a < b && (*a == ' ' || *a == '\t' || *a == '*' || *a == '-' || *a == '>' || *a == '#')) a++;
    while (b > a && (b[-1] == ' ' || b[-1] == '\t' || b[-1] == '\r')) b--;
    if (b <= a || (uint32_t)(b - a) < 4 || (uint32_t)(b - a) >= n) return 0;
    const char *p = a;
    while (p < b) {
        uint32_t by = 1, kn = um_utf8_oku(p, &by);
        if (!um_kelime_karakteri(kn) || (kn >= '0' && kn <= '9')) return 0;
        p += by ? by : 1;
    }
    char t[96];
    um_bellek_kopya(t, a, (uint64_t)(b - a)); t[b - a] = 0;
    um_kucult(c, n, t);
    return 1;
}

int ozturk_ogren(const char *metin, uint32_t boy, const char *kut) {
    if (!metin || !kut || boy < 20) return 0;
    if (!um_icinde(metin, "öz Türkçe") && !um_icinde(metin, "Öz Türkçe") && !um_icinde(metin, "karşılık") && !um_icinde(metin, "karşılığ") &&
        !um_icinde(metin, "terim") && !um_icinde(metin, "sözlük") && !um_icinde(metin, "Sözlük")) return 0;
    /* çalışma kopyası: LaTeX işaretlerini sadeleştir */
    char *w = (char *)up_ayir((uint64_t)boy + 1);
    if (!w) return 0;
    uint32_t o = 0;
    for (uint32_t i = 0; i < boy && metin[i]; ) {
        if (um_bas_mi(metin + i, "\\rightarrow")) { w[o++] = '='; i += 11; continue; }
        if (um_bas_mi(metin + i, "\\(") || um_bas_mi(metin + i, "\\)")) { i += 2; continue; }
        if (um_bas_mi(metin + i, "\xE2\x86\x92")) { w[o++] = '='; i += 3; continue; }
        if (um_bas_mi(metin + i, "->")) { w[o++] = '='; i += 2; continue; }
        if (metin[i] == ';') { w[o++] = '\n'; i++; continue; }
        w[o++] = metin[i++];
    }
    w[o] = 0;
    char *yeni = (char *)up_ayir(8192);
    if (!yeni) { up_biraktir(w); return 0; }
    yeni[0] = 0;
    int eklenen = 0;
    char *p = w;
    while (*p && eklenen < 64) {
        char *s = p;
        while (*p && *p != '\n') p++;
        char *son = p;
        if (*p) p++;
        /* "Yeni (Eski karşılığı: Eski ...)" biçimi: parantez öncesi yeni, sonrası eski sözcük */
        { char ks = *son; *son = 0;
          const char *ek = um_icinde(s, "Eski karşılığı:");
          const char *par = ek ? um_icinde(s, "(") : 0;
          if (ek && par && par < ek) {
              char eski[48], yenic[48];
              const char *r = ek + um_uzun("Eski karşılığı:"); while (*r == ' ') r++;
              const char *re = r;
              while (*re) { uint32_t by = 1, kn = um_utf8_oku(re, &by); if (!um_kelime_karakteri(kn)) break; re += by ? by : 1; }
              const char *rs = re; while (*rs == ' ') rs++;
              int tek_eski = salt_sozcuk(r, re, eski, sizeof eski) &&
                  !((*rs >= 'a' && *rs <= 'z') || ((uint8_t)*rs >= 0xC3 && (uint8_t)*rs <= 0xC5));
              /* yeni taraf: parantezden önceki ilk sözcük (a / b ise ilki) */
              const char *a = s; while (a < par && (*a == ' ' || *a == '*' || *a == '-' || *a == '\t')) a++;
              const char *ae = a;
              while (ae < par) { uint32_t by = 1, kn = um_utf8_oku(ae, &by); if (!um_kelime_karakteri(kn)) break; ae += by ? by : 1; }
              if (tek_eski && salt_sozcuk(a, ae, yenic, sizeof yenic) && !um_esit(eski, yenic)) {
                  ozturk_bulgu_t b1[1];
                  if (ozturk_tara(eski, b1, 1) == 0 && !um_icinde(yeni, eski)) {
                      um_ekle(yeni, 8192, eski); um_ekle(yeni, 8192, "="); um_ekle(yeni, 8192, yenic); um_ekle(yeni, 8192, "\n");
                      eklenen++;
                  }
              }
              *son = ks; continue;
          }
          *son = ks; }
        char *e = s;
        while (e < son && *e != '=') e++;
        if (e >= son) continue;
        /* "etiket: sol = sağ" ise etiketi at */
        char *sol0 = s;
        for (char *q = s; q < e; q++) if (*q == ':') sol0 = q + 1;
        char sol[48], sag[48];
        if (!salt_sozcuk(sol0, e, sol, sizeof sol)) continue;
        /* sağ: ilk sözcük; ardından harf olmayan bir şey (boşluk, kaynak imi, nokta) gelmeli */
        char *r = e + 1;
        while (r < son && (*r == ' ' || *r == '\t')) r++;
        char *re = r;
        while (re < son) { uint32_t by = 1, kn = um_utf8_oku(re, &by); if (!um_kelime_karakteri(kn)) break; re += by ? by : 1; }
        char *rest = re;
        while (rest < son && (*rest == ' ' || *rest == '\t')) rest++;
        if (rest < son && ((*rest >= 'a' && *rest <= 'z') || ((uint8_t)*rest >= 0xC3 && (uint8_t)*rest <= 0xC5))) continue; /* çok sözcüklü sağ taraf */
        if (!salt_sozcuk(r, re, sag, sizeof sag) || um_esit(sol, sag)) continue;
        ozturk_bulgu_t b1[1];
        if (ozturk_tara(sol, b1, 1) > 0) continue;               /* zaten biliniyor */
        if (um_icinde(yeni, sol)) continue;                       /* bu turda eklendi */
        um_ekle(yeni, 8192, sol); um_ekle(yeni, 8192, "="); um_ekle(yeni, 8192, sag); um_ekle(yeni, 8192, "\n");
        eklenen++;
    }
    if (eklenen) {
        char yol[320]; um_bicim(yol, sizeof yol, "%s/ozturkce.txt", kut);
        int64_t eb = up_boy(yol); if (eb < 0) eb = 0;
        uint64_t yb = um_uzun(yeni);
        char *tum = (char *)up_ayir((uint64_t)eb + yb + 1);
        if (tum) {
            int64_t okundu = 0;
            if (eb > 0) { up_tut_t t = up_ac(yol, 0); if (t != UP_GECERSIZ) { okundu = up_oku(t, tum, (uint64_t)eb); up_kapat(t); } }
            if (okundu < 0) okundu = 0;
            if (okundu > 0 && tum[okundu - 1] != '\n') tum[okundu++] = '\n';
            um_bellek_kopya(tum + okundu, yeni, yb);
            if (up_yaz_yola(yol, tum, (uint64_t)okundu + yb) < 0) eklenen = 0;
            up_biraktir(tum);
        } else eklenen = 0;
        ozturk_yukle(kut);
    }
    up_biraktir(yeni); up_biraktir(w);
    return eklenen;
}

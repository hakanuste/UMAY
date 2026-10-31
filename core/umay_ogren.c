/* =====================================================================
 * ÖĞREN — boşluğu görüp web'den kapatan modül. Tanım: include/umay_ogren.h
 *
 * Bu dosyada ağ, ortam değişkeni ya da dosya sistemi çağrısı YOKTUR;
 * hepsi port katmanından (up_*) geçer. ÖTÜKEN'de de derlenir ve orada
 * ağ olmadığı için hiçbir şey yapmaz.
 * ===================================================================== */
#include "umay_ogren.h"
#include "umay_gizli.h"
#include "umay_bilgi.h"
#include "umay_ortak.h"
#include "umay_port.h"
#include "umay_str.h"

#define OG_YANIT_MAX   (1u << 20)      /* ağdan gelen en çok bayt */
#define OG_METIN_MAX   (96u * 1024u)   /* çıkarılan düz metin */
#define OG_PARCA_BOY   2600u           /* künye dahil 3071'i aşmasın */
#define OG_PARCA_ADET  3
#define OG_YOL         384
#define OG_BOSLUK_MAX  16384u
#define OG_ANAHTAR_MAX 5
#define OG_ARALIK_MS   4000u           /* iki ağ çağrısı arası en az */
#define OG_OTURUM_AG   40              /* bir oturumda en çok anlık araştırma */
#define OG_BOSTA_MS    120000u         /* boşta öğrenme aralığı */

static ogren_getirici_t g_getir;
static int      g_ag_var = -1;
static uint64_t g_son_ms;
static int      g_oturum_ag;
static int      g_oturum_parca;
static uint64_t g_bosta_ms;
static int      g_bosta_basladi;

/* yazılan web belgeleri: (alan, özet, parça no) */
#define OG_YAZILAN_MAX 32
static struct { char alan[16]; char h[9]; uint8_t no; } g_yazilan[OG_YAZILAN_MAX];
static int g_yazilan_n;
void ogren_yazilan_sifirla(void) { g_yazilan_n = 0; }
int ogren_yazilan(int i, char *yol, uint32_t n) {
    if (i < 0 || i >= g_yazilan_n) return UM_YOK;
    char ad[48];
    um_bicim(ad, sizeof ad, "web-%s-%d.txt", g_yazilan[i].h, (int)g_yazilan[i].no);
    const char *kok = ayar_metin("kutuphane", "kut");
    um_bicim(yol, n, "%s/%s/%s", kok, g_yazilan[i].alan, ad);
    return UM_OLDU;
}

void ogren_getirici_ata(ogren_getirici_t f) { g_getir = f; g_ag_var = -1; }

int ogren_acik(void) {
    const char *e = up_ortam("UMAY_OGREN");
    if (e && e[0] == '0') return 0;
    if (g_getir) return 1;
    if (g_ag_var < 0) g_ag_var = up_ag_var_mi() ? 1 : 0;
    return g_ag_var;
}

int ogren_oturum_parca(void) { return g_oturum_parca; }

/* ------------------------------------------------------------------ */
/* küçük yardımcılar                                                    */
/* ------------------------------------------------------------------ */
static uint32_t fnv(const char *s) {
    uint32_t h = 2166136261u;
    for (; *s; s++) { h ^= (uint8_t)*s; h *= 16777619u; }
    return h;
}
static void hex8(uint32_t h, char *c) {
    static const char H[] = "0123456789abcdef";
    for (int i = 0; i < 8; i++) c[i] = H[(h >> (28 - 4 * i)) & 15u];
    c[8] = 0;
}

/* UTF-8 sınırında keser (dizge n-1 bayta sığar) */
static void kisalt(char *s, uint32_t n) {
    uint32_t u = um_uzun(s);
    if (u < n) return;
    uint32_t k = n - 1;
    while (k > 0 && ((uint8_t)s[k] & 0xC0u) == 0x80u) k--;
    s[k] = 0;
}

static const char *dizin_adi(const char *kat) {
    if (kat) for (int i = 0; bilgi_alan_dizini(i); i++)
        if (um_esit(kat, bilgi_alan_dizini(i))) return bilgi_alan_dizini(i);
    return "ortak";
}

/* "araştır", "derle" gibi KOMUT sözcükleri konu değildir; aramaya girmez. */
int ogren_komut_sozcugu(const char *k) {
    static const char *KOMUT[] = { "araştır", "arastir", "araştırma", "derle", "tara", "incele",
                                   "kaynak", "bul", "ara", "rapor", 0 };
    for (int i = 0; KOMUT[i]; i++) if (um_esit(k, KOMUT[i])) return 1;
    return 0;
}

/* soruyu anahtar sözcüklere indirger: "gunes paneli verimi" */
static int anahtar_kur(const char *soru, char *cik, uint32_t n) {
    char ham[OG_ANAHTAR_MAX + 4][24];
    cik[0] = 0;
    int h = soru ? bilgi_kelime_ayikla(soru, ham, OG_ANAHTAR_MAX + 4) : 0, k = 0;
    for (int i = 0; i < h && k < OG_ANAHTAR_MAX; i++) {
        if (ogren_komut_sozcugu(ham[i])) continue;
        if (k) um_ekle(cik, n, " ");
        um_ekle(cik, n, ham[i]);
        k++;
    }
    return k;
}

static void kok_yolu(char *cik, uint32_t n, const char *alt, const char *ad) {
    const char *kok = ayar_metin("kutuphane", "kut");
    if (alt && ad) um_bicim(cik, n, "%s/%s/%s", kok, alt, ad);
    else if (alt)  um_bicim(cik, n, "%s/%s", kok, alt);
    else           um_bicim(cik, n, "%s", kok);
}

static int64_t dosya_oku(const char *yol, char *t, uint32_t n) {
    t[0] = 0;
    up_tut_t h = up_ac(yol, UP_OKU);
    if (h < 0) return -1;
    int64_t k = up_oku(h, t, n - 1);
    up_kapat(h);
    if (k < 0) k = 0;
    t[k] = 0;
    return k;
}

/* ------------------------------------------------------------------ */
/* saf işlevler (sınanır)                                               */
/* ------------------------------------------------------------------ */
uint32_t ogren_url_kodla(const char *s, char *cik, uint32_t n) {
    static const char H[] = "0123456789ABCDEF";
    uint32_t o = 0;
    for (; s && *s && o + 4 < n; s++) {
        uint8_t c = (uint8_t)*s;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '.' || c == '_' || c == '~') cik[o++] = (char)c;
        else { cik[o++] = '%'; cik[o++] = H[c >> 4]; cik[o++] = H[c & 15]; }
    }
    if (n) cik[o] = 0;
    return o;
}

static int onaltilik(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int ogren_json_dizge(const char *json, const char *anahtar, char *cik, uint32_t n) {
    if (!json || !anahtar || !cik || n < 2) return UM_YANLIS;
    cik[0] = 0;
    char ara[64];
    um_bicim(ara, sizeof ara, "\"%s\"", anahtar);
    const char *p = um_icinde(json, ara);
    if (!p) return UM_YOK;
    p += um_uzun(ara);
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p != ':') return UM_YOK;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p != '"') return UM_YOK;
    p++;
    uint32_t o = 0;
    while (*p && *p != '"') {
        uint32_t cp;
        if (*p == '\\') {
            p++;
            switch (*p) {
            case 'n': cp = '\n'; p++; break;
            case 't': cp = ' ';  p++; break;
            case 'r': cp = ' ';  p++; break;
            case 'b': case 'f': cp = ' '; p++; break;
            case '/': cp = '/';  p++; break;
            case '"': cp = '"';  p++; break;
            case '\\': cp = '\\'; p++; break;
            case 'u': {
                uint32_t v = 0; int ok = 1;
                for (int i = 1; i <= 4; i++) {
                    int d = onaltilik(p[i]);
                    if (d < 0) { ok = 0; break; }
                    v = (v << 4) | (uint32_t)d;
                }
                if (!ok) { cp = '?'; p++; break; }
                p += 5;
                if (v >= 0xD800 && v <= 0xDBFF && p[0] == '\\' && p[1] == 'u') {
                    uint32_t w = 0; int ok2 = 1;
                    for (int i = 2; i <= 5; i++) {
                        int d = onaltilik(p[i]);
                        if (d < 0) { ok2 = 0; break; }
                        w = (w << 4) | (uint32_t)d;
                    }
                    if (ok2 && w >= 0xDC00 && w <= 0xDFFF) {
                        v = 0x10000u + ((v - 0xD800u) << 10) + (w - 0xDC00u);
                        p += 6;
                    }
                }
                if (v >= 0xD800 && v <= 0xDFFF) v = '?';   /* yalnız vekil */
                if (v < 32) v = ' ';
                cp = v;
                break;
            }
            default: cp = '?'; if (*p) p++; break;
            }
        } else {
            uint32_t b;
            cp = um_utf8_oku(p, &b);
            if (b == 0) b = 1;
            if (cp < 32 && cp != '\n') cp = ' ';
            p += b;
        }
        char y[5];
        uint32_t yb = um_utf8_yaz(cp, y);
        if (o + yb + 1 > n) break;
        um_bellek_kopya(cik + o, y, yb);
        o += yb;
    }
    cik[o] = 0;
    return (int)o;
}

static int varlik_coz(const char *p, uint32_t *atla, uint32_t *kn) {
    /* p '&' üzerindedir */
    static const struct { const char *ad; uint32_t kn; } V[] = {
        {"&amp;", '&'}, {"&lt;", '<'}, {"&gt;", '>'}, {"&quot;", '"'},
        {"&apos;", '\''}, {"&#39;", '\''}, {"&nbsp;", ' '}, {"&#x27;", '\''},
        {"&hellip;", 0x2026}, {"&ndash;", 0x2013}, {"&mdash;", 0x2014}
    };
    for (unsigned i = 0; i < sizeof V / sizeof V[0]; i++)
        if (um_bas_mi(p, V[i].ad)) { *atla = um_uzun(V[i].ad); *kn = V[i].kn; return 1; }
    if (p[1] == '#') {
        uint32_t v = 0, i = 2; int hex = 0;
        if (p[i] == 'x' || p[i] == 'X') { hex = 1; i++; }
        uint32_t basla = i;
        for (; i < 12; i++) {
            int d = hex ? onaltilik(p[i]) : ((p[i] >= '0' && p[i] <= '9') ? p[i] - '0' : -1);
            if (d < 0) break;
            v = v * (hex ? 16u : 10u) + (uint32_t)d;
        }
        if (i > basla && p[i] == ';' && v > 0 && v < 0x110000u) {
            *atla = i + 1; *kn = (v < 32 ? ' ' : v); return 1;
        }
    }
    return 0;
}

uint32_t ogren_html_temizle(const char *kaynak, uint32_t uzun, char *cik, uint32_t n) {
    uint32_t o = 0;
    int bosluk = 1;                         /* baştaki boşluğu at */
    if (!n) return 0;
    for (uint32_t i = 0; i < uzun && kaynak[i] && o + 6 < n;) {
        char c = kaynak[i];
        if (c == '<') {
            while (i < uzun && kaynak[i] && kaynak[i] != '>') i++;
            if (i < uzun && kaynak[i] == '>') i++;
            if (!bosluk) { cik[o++] = ' '; bosluk = 1; }
            continue;
        }
        uint32_t kn;
        if (c == '&') {
            uint32_t atla;
            if (varlik_coz(kaynak + i, &atla, &kn)) i += atla;
            else { kn = '&'; i++; }
        } else {
            uint32_t b;
            kn = um_utf8_oku(kaynak + i, &b);
            if (b == 0) b = 1;
            i += b;
        }
        if (kn == ' ' || kn == '\n' || kn == '\t' || kn == '\r' || kn < 32 || kn == 0xA0) {
            if (!bosluk) { cik[o++] = ' '; bosluk = 1; }
            continue;
        }
        char y[5];
        uint32_t yb = um_utf8_yaz(kn, y);
        um_bellek_kopya(cik + o, y, yb);
        o += yb;
        bosluk = 0;
    }
    while (o > 0 && cik[o - 1] == ' ') o--;
    cik[o] = 0;
    return o;
}

int ogren_parcala(const char *metin, uint32_t en_cok, uint32_t *bas, uint32_t *uz, int en_cok_parca) {
    uint32_t len = metin ? um_uzun(metin) : 0, pos = 0;
    int say = 0;
    while (pos < len && say < en_cok_parca) {
        while (pos < len && (metin[pos] == '\n' || metin[pos] == ' ')) pos++;
        if (pos >= len) break;
        uint32_t son = pos + en_cok;
        if (son >= len) son = len;
        else {
            uint32_t yarim = pos + en_cok / 2, k = 0;
            for (uint32_t i = son; i > yarim && !k; i--)           /* paragraf */
                if (metin[i - 1] == '\n' && metin[i - 2] == '\n') k = i;
            for (uint32_t i = son; i > yarim && !k; i--)           /* satır */
                if (metin[i - 1] == '\n') k = i;
            for (uint32_t i = son; i > yarim && !k; i--)           /* cümle */
                if (metin[i - 1] == ' ' && (metin[i - 2] == '.' || metin[i - 2] == '!' || metin[i - 2] == '?')) k = i;
            if (k) son = k;
            while (son > pos && ((uint8_t)metin[son] & 0xC0u) == 0x80u) son--;
        }
        uint32_t e = son;
        while (e > pos && (metin[e - 1] == '\n' || metin[e - 1] == ' ')) e--;
        if (e > pos) { bas[say] = pos; uz[say] = e - pos; say++; }
        pos = son;
    }
    return say;
}

const char *ogren_dil_kestir(const char *metin) {
    int harf = 0, kelime = 0;
    for (const char *p = metin; p && *p; p++) {
        uint8_t a = (uint8_t)p[0], b = (uint8_t)p[1];
        if (a == 0xC3 && (b == 0xA7 || b == 0xB6 || b == 0xBC || b == 0x87 || b == 0x96 || b == 0x9C)) harf++;
        else if (a == 0xC4 && (b == 0x9F || b == 0xB1 || b == 0x9E || b == 0xB0)) harf++;
        else if (a == 0xC5 && (b == 0x9F || b == 0x9E)) harf++;
    }
    static const char *K[] = { " ve ", " bir ", " için ", " ile ", " olan ", " bu " };
    for (unsigned i = 0; i < sizeof K / sizeof K[0]; i++)
        if (um_icinde(metin, K[i])) kelime++;
    return (harf >= 3 || kelime >= 2) ? "tr" : "en";
}

/* ------------------------------------------------------------------ */
/* boşluk / bitti çizelgeleri                                           */
/* ------------------------------------------------------------------ */
static void ogren_dizini(char *yol, uint32_t n, const char *ad) {
    kok_yolu(yol, n, "_ogren", ad);
}

static int satirda_var(const char *t, const char *hash) {
    /* satır başında "hash" ve ardından \t ya da \n */
    const char *p = t;
    while (*p) {
        if (um_esit_n(p, hash, 8) && (p[8] == '\t' || p[8] == '\n' || p[8] == 0)) return 1;
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
    }
    return 0;
}

static int bitti_mi(const char *hash) {
    char yol[OG_YOL];
    char *t = (char *)up_ayir(OG_BOSLUK_MAX + 1);
    if (!t) return 0;
    ogren_dizini(yol, sizeof yol, "bitti.txt");
    int var = 0;
    if (dosya_oku(yol, t, OG_BOSLUK_MAX + 1) >= 0) var = satirda_var(t, hash);
    up_biraktir(t);
    return var;
}

static void bitti_yaz(const char *hash) {
    char yol[OG_YOL];
    char *t = (char *)up_ayir(OG_BOSLUK_MAX + 64);
    if (!t) return;
    ogren_dizini(yol, sizeof yol, "bitti.txt");
    int64_t k = dosya_oku(yol, t, OG_BOSLUK_MAX);
    if (k < 0) k = 0;
    if (satirda_var(t, hash)) { up_biraktir(t); return; }
    if ((uint32_t)k + 10 >= OG_BOSLUK_MAX) {         /* dolunca eski yarıyı at */
        uint32_t at = (uint32_t)k / 2;
        while (at < (uint32_t)k && t[at] != '\n') at++;
        if (at < (uint32_t)k) at++;
        um_bellek_kopya(t, t + at, (uint64_t)k - at + 1);
        k -= at;
    }
    um_bicim(t + k, 16, "%s\n", hash);
    up_yaz_yola(yol, t, um_uzun(t));
    up_biraktir(t);
}

int ogren_bosluk_kaydet(const char *kategori, const char *soru) {
    if (!ogren_acik()) return UM_YOK;
    char anahtar[160];
    if (anahtar_kur(soru, anahtar, sizeof anahtar) == 0) return UM_YANLIS;
    char h[9]; hex8(fnv(anahtar), h);
    if (bitti_mi(h)) return UM_YOK;

    char yol[OG_YOL];
    char *t = (char *)up_ayir(OG_BOSLUK_MAX + 400);
    if (!t) return UM_DOLU;
    kok_yolu(yol, sizeof yol, "_ogren", 0);
    up_dizin_yarat(yol);
    ogren_dizini(yol, sizeof yol, "bosluk.txt");
    int64_t k = dosya_oku(yol, t, OG_BOSLUK_MAX);
    int sonuc = UM_OLDU;
    if (k < 0) k = 0;
    if (satirda_var(t, h)) sonuc = UM_YOK;
    else if ((uint32_t)k + 300 >= OG_BOSLUK_MAX) sonuc = UM_DOLU;
    else {
        um_bicim(t + k, 320, "%s\t%s\t%s\n", h, dizin_adi(kategori), anahtar);
        if (up_yaz_yola(yol, t, um_uzun(t)) < 0) sonuc = UM_YANLIS;
    }
    up_biraktir(t);
    return sonuc;
}

int ogren_bekleyen(void) {
    char yol[OG_YOL];
    char *b = (char *)up_ayir(2 * (OG_BOSLUK_MAX + 1));
    if (!b) return 0;
    char *d = b + OG_BOSLUK_MAX + 1;
    ogren_dizini(yol, sizeof yol, "bosluk.txt");
    if (dosya_oku(yol, b, OG_BOSLUK_MAX + 1) <= 0) { up_biraktir(b); return 0; }
    ogren_dizini(yol, sizeof yol, "bitti.txt");
    if (dosya_oku(yol, d, OG_BOSLUK_MAX + 1) < 0) d[0] = 0;
    int n = 0;
    for (const char *p = b; *p;) {
        char h[9]; um_kopya(h, sizeof h, p);
        if (um_uzun(h) == 8 && !satirda_var(d, h)) n++;
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
    }
    up_biraktir(b);
    return n;
}

/* ------------------------------------------------------------------ */
/* ağ ve ayrıştırma                                                     */
/* ------------------------------------------------------------------ */
static int64_t getir(const char *url, char *cik) {
    if (gizli_adres_mi(url)) return -1;      /* GİZLİLİK DUVARI */
    if (g_getir) return g_getir(url, cik, OG_YANIT_MAX);
    return up_ag_getir(url, cik, OG_YANIT_MAX, 10);
}

int64_t ogren_ag_getir(const char *url, char *cik, uint32_t n) {
    if (!ogren_acik()) return -1;
    if (gizli_adres_mi(url)) return -1;      /* GİZLİLİK DUVARI: kişisel sözcük geçen istek gitmez */
    if (g_getir) return g_getir(url, cik, n);
    return up_ag_getir(url, cik, n, 12);
}

static void bekle_hiz(void) {
    uint64_t s = up_ms();
    if (g_son_ms && s - g_son_ms < OG_ARALIK_MS && !g_getir) up_uyu((uint32_t)(OG_ARALIK_MS - (s - g_son_ms)));
    g_son_ms = up_ms();
}

/* Vikipedi: en iyi sayfanın düz metni. Dönüş: >0 metin uzunluğu, 0 yok, <0 ağ hatası */
static int vikipedi(const char *q, char *yanit, char *metin, char *baslik, uint32_t bn, char *kaynak, uint32_t kn) {
    const char *taban = up_ortam("UMAY_AG_WIKI");
    char kod[200], url[OG_YOL * 2];
    ogren_url_kodla(q, kod, sizeof kod);
    um_bicim(url, sizeof url,
        "%s/w/api.php?action=query&format=json&formatversion=2&generator=search"
        "&gsrnamespace=0&gsrlimit=1&gsrsearch=%s&prop=extracts&explaintext=1&exlimit=1&redirects=1",
        taban ? taban : "https://tr.wikipedia.org", kod);
    for (int deneme = 0; deneme < 2; deneme++) {
        bekle_hiz();
        int64_t k = getir(url, yanit);
        if (k < 0 && deneme == 0) {                  /* büyük yanıt olabilir: yalnız giriş */
            um_ekle(url, sizeof url, "&exintro=1");
            continue;
        }
        if (k < 0) return -1;
        if (ogren_json_dizge(yanit, "extract", metin, OG_METIN_MAX) <= 0) return 0;
        if (ogren_json_dizge(yanit, "title", baslik, bn) <= 0) um_kopya(baslik, bn, q);
        char ad[120]; um_kopya(ad, sizeof ad, baslik);
        for (char *c = ad; *c; c++) if (*c == ' ') *c = '_';
        um_bicim(kaynak, kn, "web: %s/wiki/%s (doğrulanmamış)",
                 taban ? taban : "https://tr.wikipedia.org", ad);
        return (int)um_uzun(metin);
    }
    return -1;
}

static const char *ara_dizge(const char *s, const char *igne, const char *son) {
    uint32_t n = um_uzun(igne);
    for (; *s && s < son; s++) if (um_esit_n(s, igne, n)) return s;
    return 0;
}

/* DuckDuckGo HTML sonuçları: başlık + özet birleştirilir. */
static int duckduckgo(const char *q, char *yanit, char *metin, char *baslik, uint32_t bn, char *kaynak, uint32_t kn) {
    const char *taban = up_ortam("UMAY_AG_DDG");
    char kod[200], url[OG_YOL * 2];
    ogren_url_kodla(q, kod, sizeof kod);
    um_bicim(url, sizeof url, "%s?q=%s", taban ? taban : "https://html.duckduckgo.com/html/", kod);
    bekle_hiz();
    int64_t k = getir(url, yanit);
    if (k < 0) return -1;
    uint32_t len = (uint32_t)k, o = 0, sonuc = 0;
    metin[0] = 0;
    const char *son = yanit + len;
    const char *p = yanit;
    char ilk_url[200]; ilk_url[0] = 0;
    while (sonuc < 5 && p < son) {
        const char *a = ara_dizge(p, "result__a", son);
        if (!a) break;
        const char *a_son = ara_dizge(a, "</a>", son);
        const char *g = ara_dizge(a, ">", son);
        if (!a_son || !g || g > a_son) break;
        char tb[160];
        ogren_html_temizle(g + 1, (uint32_t)(a_son - g - 1), tb, sizeof tb);
        if (!ilk_url[0]) {
            const char *u = ara_dizge(a, "uddg=", g);
            if (u) {
                u += 5; uint32_t j = 0;
                while (*u && *u != '&' && *u != '"' && j + 1 < sizeof ilk_url) {
                    if (*u == '%' && onaltilik(u[1]) >= 0 && onaltilik(u[2]) >= 0) {
                        ilk_url[j++] = (char)(onaltilik(u[1]) * 16 + onaltilik(u[2])); u += 3;
                    } else ilk_url[j++] = *u++;
                }
                ilk_url[j] = 0;
            }
        }
        const char *sonraki = ara_dizge(a_son, "result__a", son);
        const char *oz = ara_dizge(a_son, "result__snippet", sonraki ? sonraki : son);
        char sb[700]; sb[0] = 0;
        if (oz) {
            const char *oz_g = ara_dizge(oz, ">", son);
            const char *oz_s = oz_g ? ara_dizge(oz_g, "</a>", son) : 0;
            if (oz_g && oz_s) ogren_html_temizle(oz_g + 1, (uint32_t)(oz_s - oz_g - 1), sb, sizeof sb);
        }
        if (sb[0]) {
            o += um_bicim(metin + o, OG_METIN_MAX - o, "%s: %s\n\n", tb, sb);
            sonuc++;
        }
        p = a_son;
    }
    if (!sonuc) return 0;
    um_kopya(baslik, bn, q);
    /* özet kaynağı: ilk bağlantı yalnız alan adı olarak anılır */
    if (ilk_url[0] && (um_bas_mi(ilk_url, "http://") || um_bas_mi(ilk_url, "https://")))
        um_bicim(kaynak, kn, "web: DuckDuckGo özeti, ilk sonuç %s (doğrulanmamış)", ilk_url);
    else
        um_bicim(kaynak, kn, "web: DuckDuckGo arama özeti (doğrulanmamış)");
    return (int)o;
}

/* metin konuyla ilgisiz mi? (anahtarlardan en az biri geçmeli) */
static int ilgili_mi(const char *anahtar, const char *baslik, const char *metin) {
    char ham[1400], kucuk[1400];
    um_bicim(ham, sizeof ham, "%s %s", baslik, metin);
    um_kucult(kucuk, sizeof kucuk, ham);
    char kel[OG_ANAHTAR_MAX][24];
    int n = bilgi_kelime_ayikla(anahtar, kel, OG_ANAHTAR_MAX);
    for (int i = 0; i < n; i++) {
        uint32_t u = um_uzun(kel[i]);
        char kok[24]; um_kopya(kok, sizeof kok, kel[i]);
        if (u > 5) { uint32_t kes = 5; while (kes > 0 && ((uint8_t)kok[kes] & 0xC0u) == 0x80u) kes--; kok[kes] = 0; }
        if (kok[0] && um_icinde(kucuk, kok)) return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* araştırma                                                            */
/* ------------------------------------------------------------------ */
int ogren_arastir(const char *kategori, const char *konu) {
    char anahtar[160];
    if (anahtar_kur(konu, anahtar, sizeof anahtar) == 0) return UM_YANLIS;
    if (!ogren_acik()) return UM_YOK;
    char h[9]; hex8(fnv(anahtar), h);
    const char *alan = dizin_adi(kategori);

    char yol[OG_YOL], ad[48];
    um_bicim(ad, sizeof ad, "web-%s-1.txt", h);
    kok_yolu(yol, sizeof yol, alan, ad);
    if (up_var_mi(yol)) { bitti_yaz(h); return 0; }       /* zaten öğrenilmiş */

    char *yanit = (char *)up_ayir(OG_YANIT_MAX + 1);
    char *metin = (char *)up_ayir(OG_METIN_MAX + 1);
    if (!yanit || !metin) { if (yanit) up_biraktir(yanit); if (metin) up_biraktir(metin); return UM_DOLU; }
    char baslik[96], kaynak[200];
    baslik[0] = 0; kaynak[0] = 0; metin[0] = 0;

    int r = vikipedi(anahtar, yanit, metin, baslik, sizeof baslik, kaynak, sizeof kaynak);
    if (r > 0 && !ilgili_mi(anahtar, baslik, metin)) r = 0;
    int ag_hata = (r < 0);
    if (r <= 0) {
        int r2 = duckduckgo(anahtar, yanit, metin, baslik, sizeof baslik, kaynak, sizeof kaynak);
        if (r2 > 0) r = r2; else if (r2 == 0) r = 0; else r = ag_hata ? -1 : 0;
    }
    if (r < 0) { up_biraktir(yanit); up_biraktir(metin); return UM_YOK; }  /* ağ yok: boşluk beklesin */

    int eklenen = 0;
    if (r > 0) {
        /* metin düz yazıya: satır içi boşlukları sadeleştir */
        uint32_t bas[OG_PARCA_ADET], uz[OG_PARCA_ADET];
        int say = ogren_parcala(metin, OG_PARCA_BOY, bas, uz, OG_PARCA_ADET);
        kisalt(baslik, 60);
        kisalt(kaynak, 90);
        const char *dil = ogren_dil_kestir(metin);
        for (int i = 0; i < say; i++) {
            char parca[OG_PARCA_BOY + 8];
            uint32_t pn = uz[i] < OG_PARCA_BOY ? uz[i] : OG_PARCA_BOY;
            um_bellek_kopya(parca, metin + bas[i], pn);
            parca[pn] = 0;
            /* künye satırı sızması: gövde '#' ile başlayamaz */
            if (parca[0] == '#') parca[0] = ' ';
            char bas_ad[110];
            um_bicim(bas_ad, sizeof bas_ad, "%s (%d/%d)", baslik, i + 1, say);
            char et[40];
            { char kel[1][24]; if (bilgi_kelime_ayikla(anahtar, kel, 1) > 0) um_bicim(et, sizeof et, "web,%s", kel[0]); else um_kopya(et, sizeof et, "web"); }

            char dosya[OG_PARCA_BOY + 512];
            um_bicim(dosya, sizeof dosya,
                     "#kategori: %s\n#baslik: %s\n#etiket: %s\n#kaynak: %s\n#dil: %s\n\n%s\n",
                     alan, bas_ad, et, kaynak, dil, parca);
            um_bicim(ad, sizeof ad, "web-%s-%d.txt", h, i + 1);
            char alt[OG_YOL];
            kok_yolu(alt, sizeof alt, alan, 0);
            up_dizin_yarat(alt);                       /* yoksa aç; varsa zararsız */
            kok_yolu(yol, sizeof yol, alan, ad);
            if (up_yaz_yola(yol, dosya, um_uzun(dosya)) < 0)
                gun_yaz("ÖĞREN: yazılamadı (yalnız bellekte): %s", yol);
            else if (g_yazilan_n < OG_YAZILAN_MAX) {
                um_kopya(g_yazilan[g_yazilan_n].alan, sizeof g_yazilan[0].alan, alan);
                um_kopya(g_yazilan[g_yazilan_n].h, sizeof g_yazilan[0].h, h);
                g_yazilan[g_yazilan_n].no = (uint8_t)(i + 1);
                g_yazilan_n++;
            }
            if (bilgi_kayit_dil(alan, bas_ad, kaynak, KT_DIS, et, parca, dil, "") == UM_OLDU) eklenen++;
        }
        gun_yaz("ÖĞREN: '%s' için %d parça öğrenildi (%s).", anahtar, eklenen, kaynak);
    } else {
        gun_yaz("ÖĞREN: '%s' için web'de anlamlı sonuç bulunamadı.", anahtar);
    }
    bitti_yaz(h);
    g_oturum_parca += eklenen;
    up_biraktir(yanit);
    up_biraktir(metin);
    return eklenen;
}

int ogren_hemen(const char *kategori, const char *soru) {
    if (!ogren_acik()) return UM_YOK;
    char anahtar[160];
    if (anahtar_kur(soru, anahtar, sizeof anahtar) == 0) return UM_YANLIS;
    char h[9]; hex8(fnv(anahtar), h);
    if (bitti_mi(h)) return 0;                         /* bir kez denendi */
    ogren_bosluk_kaydet(kategori, soru);               /* ağ düşerse yarın tamamlanır */
    if (g_oturum_ag >= OG_OTURUM_AG) return 0;
    g_oturum_ag++;
    return ogren_arastir(kategori, soru);
}

int ogren_gecis(int en_cok) {
    if (!ogren_acik() || en_cok <= 0) return UM_YOK;
    char yol[OG_YOL];
    char *b = (char *)up_ayir(OG_BOSLUK_MAX + 1);
    if (!b) return 0;
    ogren_dizini(yol, sizeof yol, "bosluk.txt");
    if (dosya_oku(yol, b, OG_BOSLUK_MAX + 1) <= 0) { up_biraktir(b); return 0; }
    int toplam = 0, islenen = 0;
    char *p = b;
    while (*p && islenen < en_cok) {
        char *satir = p;
        while (*p && *p != '\n') p++;
        char son = *p; *p = 0;
        if (son) p++;
        /* hash \t kategori \t konu */
        char *t1 = satir; while (*t1 && *t1 != '\t') t1++;
        if (!*t1) continue;
        *t1++ = 0;
        char *t2 = t1; while (*t2 && *t2 != '\t') t2++;
        if (!*t2) continue;
        *t2++ = 0;
        if (um_uzun(satir) != 8 || bitti_mi(satir)) continue;
        islenen++;
        int r = ogren_arastir(t1, t2);
        if (r == UM_YOK) { up_biraktir(b); return toplam ? toplam : UM_YOK; }   /* ağ yok: dur */
        if (r > 0) toplam += r;
    }
    up_biraktir(b);
    return toplam;
}

void ogren_bosta(void) {
    if (!ogren_acik()) return;
    uint64_t s = up_ms();
    if (!g_bosta_basladi) { g_bosta_basladi = 1; g_bosta_ms = s; return; }   /* açılışta bekle */
    if (s - g_bosta_ms < OG_BOSTA_MS) return;
    g_bosta_ms = s;
    ogren_gecis(1);
}

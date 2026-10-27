#include "umay_ceviri.h"
#include "umay_bilgi.h"
#include "umay_ogren.h"
#include "umay_ortak.h"
#include "umay_port.h"
#include "umay_str.h"

#define CY_PARCA 450
#define CY_KUYRUK_DOSYA "_ceviri/bekleyen.txt"

static ceviri_motor_t g_motor;
void ceviri_motor_ata(ceviri_motor_t f) { g_motor = f; }

const char *ceviri_yontem_adi(ceviri_yontem_t y) {
    switch (y) {
    case CY_MOTOR:  return "gömülü model";
    case CY_AG:     return "web çeviri hizmeti";
    case CY_SOZLUK: return "sözlük (kelime kelime)";
    default:        return "-";
    }
}

/* ---------------------------------------------------------------- diller */
static const struct { const char *ad, *kod; } DILLER[] = {
    { "ingilizce", "en" }, { "türkçe", "tr" }, { "turkce", "tr" }, { "almanca", "de" },
    { "fransızca", "fr" }, { "rusça", "ru" }, { "arapça", "ar" }, { "ispanyolca", "es" },
    { "italyanca", "it" }, { "japonca", "ja" }, { "çince", "zh" }, { "kazakça", "kk" },
    { "özbekçe", "uz" }, { "kırgızca", "ky" }, { "azerice", "az" }, { "farsça", "fa" },
    { "moğolca", "mn" }, { "gürcüce", "ka" }, { "türkmence", "tk" }, { "korece", "ko" },
    { 0, 0 }
};

int ceviri_dil_kodu(const char *sozcuk, char *kod) {
    char k[64];
    um_kucult(k, sizeof k, sozcuk);
    for (int i = 0; DILLER[i].ad; i++) {
        uint32_t u = um_uzun(DILLER[i].ad);
        if (um_bas_mi(k, DILLER[i].ad) && um_uzun(k) <= u + 6) {
            um_kopya(kod, 8, DILLER[i].kod); return UM_OLDU;
        }
    }
    return UM_YOK;
}

static int tr_harfli(const char *m) {
    for (const char *p = m; *p; p++) {
        uint8_t c = (uint8_t)*p;
        if (c == 0xC3 && (uint8_t)p[1] != 0) { uint8_t d = (uint8_t)p[1]; if (d == 0xA7 || d == 0xB6 || d == 0xBC || d == 0x87 || d == 0x96 || d == 0x9C) return 1; }
        if (c == 0xC4 && (uint8_t)p[1] != 0) { uint8_t d = (uint8_t)p[1]; if (d == 0x9F || d == 0xB1 || d == 0x9E || d == 0xB0) return 1; }
        if (c == 0xC5 && (uint8_t)p[1] != 0) { uint8_t d = (uint8_t)p[1]; if (d == 0x9F || d == 0x9E) return 1; }
    }
    return 0;
}

static int komut_sozcugu_mu(const char *k) {
    static const char *K[] = { "çevir", "cevir", "tercüme", "çeviri", "şunu", "bunu", "metni", "cümleyi", "yazıyı",
                               "lütfen", "bana", "sözü", "olarak", "diline", 0 };
    for (int i = 0; K[i]; i++) if (um_bas_mi(k, K[i])) return 1;
    return 0;
}

int ceviri_istek_coz(const char *soz, char *kd, char *hd, char *metin, uint32_t n) {
    kd[0] = 0; hd[0] = 0; metin[0] = 0;
    char *k = (char *)up_ayir((uint64_t)um_uzun(soz) + 1);
    if (!k) return UM_YOK;
    um_kucult(k, um_uzun(soz) + 1, soz);
    /* dilleri bul: -den/-dan ekli olan kaynak, diğeri hedef */
    const char *p = k;
    while (*p) {
        for (int i = 0; DILLER[i].ad; i++) {
            uint32_t u = um_uzun(DILLER[i].ad);
            if (um_bas_mi(p, DILLER[i].ad) && (p == k || p[-1] == ' ' || p[-1] == ':' || p[-1] == '\'' || p[-1] == '"')) {
                const char *e = p + u;
                int kaynak = (e[0] == 'd' || e[0] == 't') && (e[1] == 'e' || e[1] == 'a') && e[2] == 'n';
                if (e[0] == '\'' && (e[1] == 'd' || e[1] == 't') && (e[2] == 'e' || e[2] == 'a') && e[3] == 'n') kaynak = 1;
                if (kaynak) um_kopya(kd, 8, DILLER[i].kod); else um_kopya(hd, 8, DILLER[i].kod);
            }
        }
        p++;
    }
    /* çevrilecek metin: ':' sonrası, yoksa tırnak içi, yoksa komut/dil sözcükleri atılmış kalan */
    const char *kolon = 0;
    for (const char *q = soz; *q; q++) if (*q == ':') { kolon = q + 1; break; }
    if (kolon) {
        while (*kolon == ' ') kolon++;
        um_kopya(metin, n, kolon);
    } else {
        const char *a = 0, *b = 0;
        for (const char *q = soz; *q; q++) {
            if ((*q == '"' || (uint8_t)*q == 0xE2) && !a) { a = q; }
        }
        (void)b;
        if (a && *a == '"') {
            const char *e = a + 1; while (*e && *e != '"') e++;
            uint32_t L = (uint32_t)(e - a - 1);
            if (L + 1 < n) { um_bellek_kopya(metin, a + 1, L); metin[L] = 0; }
        } else {
            metin[0] = 0;
            const char *q = soz;
            while (*q) {
                while (*q == ' ') q++;
                const char *e = q; while (*e && *e != ' ') e++;
                char tok[96]; uint32_t L = (uint32_t)(e - q); if (L >= sizeof tok) L = sizeof tok - 1;
                um_bellek_kopya(tok, q, L); tok[L] = 0;
                char kt[96]; um_kucult(kt, sizeof kt, tok);
                char kod[8];
                if (L && !komut_sozcugu_mu(kt) && ceviri_dil_kodu(kt, kod) != UM_OLDU) {
                    if (metin[0]) um_ekle(metin, n, " ");
                    um_ekle(metin, n, tok);
                }
                q = e;
            }
        }
    }
    up_biraktir(k);
    if (!kd[0]) um_kopya(kd, 8, (tr_harfli(metin) || um_esit(ogren_dil_kestir(metin), "tr")) ? "tr" : "en");
    if (!hd[0]) um_kopya(hd, 8, um_esit(kd, "tr") ? "en" : "tr");
    return metin[0] ? UM_OLDU : UM_YOK;
}

/* ---------------------------------------------------------------- sözlük */
static const char *CEKIRDEK[] = {
    "clay=kil", "straw=saman", "soil=toprak", "wall=duvar", "stone=taş", "water=su", "well=kuyu",
    "cistern=sarnıç", "compost=kompost", "worm=solucan", "seed=tohum", "greenhouse=sera", "battery=akü",
    "wind=rüzgâr", "mill=değirmen", "gear=dişli", "wheel=çark", "tool=alet", "house=ev", "roof=çatı",
    "door=kapı", "gate=kapı", "fence=çit", "tree=ağaç", "olive=zeytin", "chicken=tavuk", "goose=kaz",
    "duck=ördek", "pond=gölet", "pump=pompa", "pipe=boru", "filter=süzgeç", "sun=güneş", "solar=güneş",
    "energy=enerji", "power=güç", "radio=telsiz", "antenna=anten", "frequency=frekans", "map=harita",
    "road=yol", "mountain=dağ", "desert=çöl", "river=dere", "fire=ateş", "wood=ahşap", "sand=kum",
    "lime=kireç", "mud=çamur", "brick=tuğla", "earth=toprak", "garden=bahçe", "plant=bitki",
    "herb=ot", "oil=yağ", "salt=tuz", "bread=ekmek", "flour=un", "grain=tahıl", "dance=dans",
    "music=müzik", "history=tarih", "language=dil", "book=kitap", "engine=motor", "vehicle=araç",
    "fuel=yakıt", "drawing=çizim", "rain=yağmur", "winter=kış", "summer=yaz", "spring=bahar",
    "autumn=sonbahar", "day=gün", "night=gece", "the=", "a=", "an=",
    "is=", "are=", "of=-in", "and=ve", "or=veya", "with=ile", "for=için", "from=-den", "in=içinde",
    "on=üzerinde", "to=-e", "how=nasıl", "what=ne", "where=nerede", "when=ne zaman", "why=neden",
    "who=kim", "build=yapmak", "make=yapmak", "use=kullanmak", "mix=karıştırmak", "dry=kurutmak",
    0
};

static int sozluk_ara(const char *kd, const char *hd, const char *sozcuk, char *cik, uint32_t n, const char *dosya) {
    /* önce kullanıcı dosyası, sonra çekirdek. Satır: kaynak=hedef (en=tr); tersi için ters aranır. */
    int ters = um_esit(kd, "tr") && um_esit(hd, "en");
    if (!(um_esit(kd, "en") && um_esit(hd, "tr")) && !ters) return 0;
    for (int tur = 0; tur < 2; tur++) {
        const char *kaynak = 0; char *buf = 0; uint32_t bn = 0;
        if (tur == 0) {
            if (!dosya) continue;
            int64_t b = up_boy(dosya);
            if (b <= 0 || b > 1 << 20) continue;
            buf = (char *)up_ayir((uint64_t)b + 1); if (!buf) continue;
            up_tut_t t = up_ac(dosya, UP_OKU); if (t < 0) { up_biraktir(buf); continue; }
            int64_t r = up_oku(t, buf, (uint64_t)b); up_kapat(t);
            if (r <= 0) { up_biraktir(buf); continue; }
            buf[r] = 0; bn = (uint32_t)r; kaynak = buf;
        }
        int bulundu = 0;
        if (tur == 0 && kaynak) {
            const char *p = kaynak, *s = kaynak + bn;
            while (p < s && !bulundu) {
                const char *e = p; while (e < s && *e != '\n') e++;
                const char *eq = p; while (eq < e && *eq != '=') eq++;
                if (eq < e) {
                    const char *l0 = p, *l1 = eq, *r0 = eq + 1, *r1 = e;
                    if (ters) { const char *t0 = l0, *t1 = l1; l0 = r0; l1 = r1; r0 = t0; r1 = t1; }
                    while (r1 > r0 && (r1[-1] == '\r' || r1[-1] == ' ')) r1--;
                    uint32_t ll = (uint32_t)(l1 - l0), rl = (uint32_t)(r1 - r0);
                    if (ll == um_uzun(sozcuk)) {
                        int es = 1; for (uint32_t i = 0; i < ll; i++) if (l0[i] != sozcuk[i]) { es = 0; break; }
                        if (es && rl + 1 <= n) { um_bellek_kopya(cik, r0, rl); cik[rl] = 0; bulundu = 1; }
                    }
                }
                p = e + 1;
            }
        } else if (tur == 1) {
            for (int i = 0; CEKIRDEK[i] && !bulundu; i++) {
                const char *e = CEKIRDEK[i]; const char *eq = e; while (*eq && *eq != '=') eq++;
                const char *l0 = e, *l1 = eq, *r0 = eq + 1, *r1 = eq + 1 + um_uzun(eq + 1);
                if (ters) { const char *t0 = l0, *t1 = l1; l0 = r0; l1 = r1; r0 = t0; r1 = t1; }
                uint32_t ll = (uint32_t)(l1 - l0), rl = (uint32_t)(r1 - r0);
                if (ll && ll == um_uzun(sozcuk)) {
                    int es = 1; for (uint32_t k = 0; k < ll; k++) if (l0[k] != sozcuk[k]) { es = 0; break; }
                    if (es && rl + 1 <= n) { um_bellek_kopya(cik, r0, rl); cik[rl] = 0; bulundu = 1; }
                }
            }
        }
        if (buf) up_biraktir(buf);
        if (bulundu) return 1;
    }
    return 0;
}

static int sozluk_cevir(const char *kd, const char *hd, const char *metin, char *cik, uint32_t n) {
    if (!((um_esit(kd, "en") && um_esit(hd, "tr")) || (um_esit(kd, "tr") && um_esit(hd, "en")))) return UM_YOK;
    char dosya[UMAY_YOL_MAX];
    um_bicim(dosya, sizeof dosya, "%s/sozluk-ceviri.txt", ayar_metin("kutuphane", "kut"));
    cik[0] = 0;
    int topl = 0, bilinen = 0;               /* yalnız içerik sözcükleri (>=4 harf) sayılır */
    const char *p = metin;
    while (*p) {
        while (*p == ' ' || *p == '\n' || *p == '\t') p++;
        if (!*p) break;
        const char *e = p;
        while (*e && *e != ' ' && *e != '\n' && *e != '\t') e++;
        char tok[64]; uint32_t L = (uint32_t)(e - p);
        /* sondaki noktalama ayrılır */
        char son = 0;
        while (L > 0 && (p[L - 1] == '.' || p[L - 1] == ',' || p[L - 1] == ';' || p[L - 1] == '?' || p[L - 1] == '!' || p[L - 1] == ':')) { son = p[L - 1]; L--; }
        if (L >= sizeof tok) L = sizeof tok - 1;
        um_bellek_kopya(tok, p, L); tok[L] = 0;
        char kt[64]; um_kucult(kt, sizeof kt, tok);
        char sonuc[96];
        int icerik = L >= 4;
        if (icerik) topl++;
        if (L && sozluk_ara(kd, hd, kt, sonuc, sizeof sonuc, dosya)) {
            if (icerik) bilinen++;
            if (sonuc[0]) { if (cik[0]) um_ekle(cik, n, " "); um_ekle(cik, n, sonuc); }
        } else if (L) {
            if (cik[0]) um_ekle(cik, n, " ");
            um_ekle(cik, n, tok);
        }
        if (son) { char s2[2] = { son, 0 }; um_ekle(cik, n, s2); }
        p = e;
    }
    /* çevrilen sözcük oranı düşükse sunma */
    if (topl == 0 || bilinen * 100 < topl * 70) { cik[0] = 0; return UM_YOK; }
    return (int)um_uzun(cik);
}

/* ---------------------------------------------------------------- ağ */
static int ag_parca(const char *kd, const char *hd, const char *parca, char *cik, uint32_t n) {
    char kodlu[1600], url[2048];
    ogren_url_kodla(parca, kodlu, sizeof kodlu);
    const char *taban = ayar_metin("ag_ceviri", "https://api.mymemory.translated.net/get");
    const char *env = up_ortam("UMAY_AG_CEVIRI");
    if (env && *env) taban = env;
    um_bicim(url, sizeof url, "%s?q=%s&langpair=%s%%7C%s", taban, kodlu, kd, hd);
    char *yanit = (char *)up_ayir(1u << 16);
    if (!yanit) return UM_YOK;
    int64_t r = ogren_ag_getir(url, yanit, 1u << 16);
    int sonuc = UM_YOK;
    if (r > 0) {
        yanit[r < (1 << 16) ? r : (1 << 16) - 1] = 0;
        char t[2048];
        if (ogren_json_dizge(yanit, "translatedText", t, sizeof t) > 0 && t[0] &&
            !um_icinde(t, "MYMEMORY WARNING") && !um_icinde(t, "PLEASE SELECT") && !um_icinde(t, "INVALID")) {
            um_kopya(cik, n, t); sonuc = UM_OLDU;
        }
    }
    up_biraktir(yanit);
    return sonuc;
}

int ceviri_yap(const char *kd, const char *hd, const char *metin, char *cik, uint32_t n, ceviri_yontem_t *yontem) {
    if (yontem) *yontem = CY_YOK;
    cik[0] = 0;
    if (!metin || !*metin || um_esit(kd, hd)) return UM_YOK;
    if (g_motor && g_motor(kd, hd, metin, cik, n) > 0) { if (yontem) *yontem = CY_MOTOR; return (int)um_uzun(cik); }
    if (ogren_acik()) {
        uint32_t ml = um_uzun(metin);
        uint32_t *bas = (uint32_t *)up_ayir(512 * 4), *uz = (uint32_t *)up_ayir(512 * 4);
        if (bas && uz) {
            int np = ogren_parcala(metin, CY_PARCA, bas, uz, 512);
            int tamam = np > 0;
            for (int i = 0; i < np && tamam; i++) {
                char par[CY_PARCA + 8], cv[2200];
                um_bellek_kopya(par, metin + bas[i], uz[i]); par[uz[i]] = 0;
                if (ag_parca(kd, hd, par, cv, sizeof cv) != UM_OLDU) { tamam = 0; break; }
                if (cik[0]) um_ekle(cik, n, " ");
                um_ekle(cik, n, cv);
            }
            up_biraktir(bas); up_biraktir(uz);
            if (tamam && cik[0]) { (void)ml; if (yontem) *yontem = CY_AG; return (int)um_uzun(cik); }
        } else { if (bas) up_biraktir(bas); if (uz) up_biraktir(uz); }
        cik[0] = 0;
    }
    int r = sozluk_cevir(kd, hd, metin, cik, n);
    if (r > 0) { if (yontem) *yontem = CY_SOZLUK; return r; }
    return UM_YOK;
}

/* ---------------------------------------------------------------- kuyruk (yabancı belge → Türkçe çeviri kaydı) */
static uint32_t fnv(const char *s, uint32_t n) { uint32_t h = 2166136261u; for (uint32_t i = 0; i < n; i++) { h ^= (uint8_t)s[i]; h *= 16777619u; } return h; }
static void hex8(char *c, uint32_t h) { static const char H[] = "0123456789abcdef"; for (int i = 7; i >= 0; i--) { c[i] = H[h & 15]; h >>= 4; } c[8] = 0; }

static char *oku(const char *yol, uint32_t *uz) {
    int64_t b = up_boy(yol);
    if (b <= 0 || b > (8 << 20)) return 0;
    char *m = (char *)up_ayir((uint64_t)b + 1);
    if (!m) return 0;
    up_tut_t t = up_ac(yol, UP_OKU);
    if (t < 0) { up_biraktir(m); return 0; }
    int64_t r = up_oku(t, m, (uint64_t)b); up_kapat(t);
    if (r <= 0) { up_biraktir(m); return 0; }
    m[r] = 0; *uz = (uint32_t)r; return m;
}

static void kuyruk_yolu(char *y, uint32_t n) { um_bicim(y, n, "%s/%s", ayar_metin("kutuphane", "kut"), CY_KUYRUK_DOSYA); }

int ceviri_kayit(const char *yol) {
    char ky[UMAY_YOL_MAX]; kuyruk_yolu(ky, sizeof ky);
    char dz[UMAY_YOL_MAX]; um_bicim(dz, sizeof dz, "%s/_ceviri", ayar_metin("kutuphane", "kut")); up_dizin_yarat(dz);
    uint32_t bn = 0; char *eski = oku(ky, &bn);
    if (eski && um_icinde(eski, yol)) { up_biraktir(eski); return UM_YOK; }
    uint32_t yl = um_uzun(yol);
    char *y = (char *)up_ayir((uint64_t)bn + yl + 2);
    if (!y) { if (eski) up_biraktir(eski); return UM_YOK; }
    if (bn) um_bellek_kopya(y, eski, bn);
    um_bellek_kopya(y + bn, yol, yl); y[bn + yl] = '\n'; y[bn + yl + 1] = 0;
    up_yaz_yola(ky, y, (uint64_t)bn + yl + 1);
    up_biraktir(y); if (eski) up_biraktir(eski);
    return UM_OLDU;
}

int ceviri_bekleyen(void) {
    char ky[UMAY_YOL_MAX]; kuyruk_yolu(ky, sizeof ky);
    uint32_t bn = 0; char *b = oku(ky, &bn);
    if (!b) return 0;
    int n = 0; for (uint32_t i = 0; i < bn; i++) if (b[i] == '\n') n++;
    up_biraktir(b);
    return n;
}

static int kunye_al(const char *m, const char *anahtar, char *c, uint32_t n) {
    char ara[32]; um_bicim(ara, sizeof ara, "#%s:", anahtar);
    const char *p = m;
    while (*p == '#') {
        const char *e = p; while (*e && *e != '\n') e++;
        if (um_bas_mi(p, ara)) {
            p += um_uzun(ara); while (*p == ' ') p++;
            uint32_t L = (uint32_t)(e - p); while (L && (p[L - 1] == '\r' || p[L - 1] == ' ')) L--;
            if (L >= n) L = n - 1;
            um_bellek_kopya(c, p, L); c[L] = 0; return 1;
        }
        p = *e ? e + 1 : e;
    }
    c[0] = 0; return 0;
}

/* tek belgeyi çevirir; 1 = yazıldı, 0 = gerek yok/zaten var, -1 = şimdilik olmadı (ağ yok) */
static int belge_cevir(const char *yol) {
    uint32_t bn = 0; char *m = oku(yol, &bn);
    if (!m) return 0;
    char dil[8], kat[32], bas[96], et[96], kay[96];
    kunye_al(m, "dil", dil, sizeof dil); kunye_al(m, "kategori", kat, sizeof kat);
    kunye_al(m, "baslik", bas, sizeof bas); kunye_al(m, "etiket", et, sizeof et); kunye_al(m, "kaynak", kay, sizeof kay);
    const char *govde = m;
    while (*govde == '#') { while (*govde && *govde != '\n') govde++; if (*govde) govde++; }
    while (*govde == '\n' || *govde == '\r') govde++;
    if (!dil[0]) um_kopya(dil, sizeof dil, ogren_dil_kestir(govde));
    if (um_esit(dil, "tr") || !kat[0] || !*govde) { up_biraktir(m); return 0; }
    uint32_t h = fnv(m, bn); char hx[16]; hex8(hx, h);
    char hedef[UMAY_YOL_MAX];
    um_bicim(hedef, sizeof hedef, "%s/%s/ceviri-%s.txt", ayar_metin("kutuphane", "kut"), kat, hx);
    if (up_boy(hedef) > 0) { up_biraktir(m); return 0; }
    uint32_t cap = 4u * (uint32_t)um_uzun(govde) + 4096;
    char *cev = (char *)up_ayir(cap);
    if (!cev) { up_biraktir(m); return -1; }
    ceviri_yontem_t y;
    int r = ceviri_yap(dil, "tr", govde, cev, cap, &y);
    int sonuc = -1;
    if (r > 0 && (y == CY_AG || y == CY_MOTOR)) {
        uint32_t cc = cap + 1024;
        char *b = (char *)up_ayir(cc);
        if (b) {
            char yb[128]; um_bicim(yb, sizeof yb, "%s (Türkçe çeviri)", bas);
            char ke[160]; um_bicim(ke, sizeof ke, "%s (%s; makine çevirisi, doğrulanmamış)", yol, ceviri_yontem_adi(y));
            um_bicim(b, cc, "#kategori: %s\n#baslik: %s\n#etiket: %s,ceviri\n#kaynak: %s\n#dil: tr\n#ozgun_dil: %s\n\n", kat, yb, et[0] ? et : "ceviri", ke, dil);
            um_ekle(b, cc, cev);
            if (up_yaz_yola(hedef, b, um_uzun(b)) >= 0) { bilgi_belge_ekle(hedef); sonuc = 1; }
            up_biraktir(b);
        }
    }
    up_biraktir(cev); up_biraktir(m);
    return sonuc;
}

int ceviri_gecis(int en_cok) {
    char ky[UMAY_YOL_MAX]; kuyruk_yolu(ky, sizeof ky);
    uint32_t bn = 0; char *b = oku(ky, &bn);
    if (!b) return 0;
    char *kalan = (char *)up_ayir((uint64_t)bn + 1);
    if (!kalan) { up_biraktir(b); return 0; }
    uint32_t kn = 0; int yazilan = 0, islenen = 0;
    const char *p = b, *s = b + bn;
    while (p < s) {
        const char *e = p; while (e < s && *e != '\n') e++;
        uint32_t L = (uint32_t)(e - p);
        if (L) {
            char yol[UMAY_YOL_MAX]; if (L >= sizeof yol) L = sizeof yol - 1;
            um_bellek_kopya(yol, p, L); yol[L] = 0;
            int bitti = 0;
            if (islenen < en_cok) {
                islenen++;
                int r = belge_cevir(yol);
                if (r >= 0) { bitti = 1; if (r > 0) yazilan++; }
                else { islenen = en_cok; }            /* ağ yok: kalanı bir sonraya bırak */
            }
            if (!bitti) { um_bellek_kopya(kalan + kn, p, (uint32_t)(e - p)); kn += (uint32_t)(e - p); kalan[kn++] = '\n'; }
        }
        p = e + 1;
    }
    kalan[kn] = 0;
    up_yaz_yola(ky, kalan, kn);
    up_biraktir(kalan); up_biraktir(b);
    return yazilan;
}

void ceviri_bosta(void) {
    static uint64_t son;
    uint64_t t = up_ms();
    if (son && t - son < 60000) return;
    son = t;
    if (ceviri_bekleyen() <= 0 || !ogren_acik()) return;
    int n = ceviri_gecis(3);
    if (n > 0) gun_yaz("ÇEVİRMEN: %d belge Türkçeye çevrildi (makine çevirisi, doğrulanmamış).", n);
}

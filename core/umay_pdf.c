#include "umay_pdf.h"
#include "umay_ortak.h"
#include "umay_port.h"
#include "umay_str.h"

#define PDF_NESNE_MAX   262144u
#define PDF_CIKTI_MAX   (8u * 1024u * 1024u)
#define PDF_AKIS_MAX    (64u * 1024u * 1024u)
#define PDF_YAZI_MAX    256
#define PDF_DERIN_MAX   24

/* ---------------------------------------------------------------- bayt tamponu */
typedef struct { uint8_t *p; uint32_t n, kap; int tasti; uint32_t sinir; } tampon_t;

static int tp_ek(tampon_t *t, const void *v, uint32_t k) {
    if (t->tasti) return 0;
    if (t->n + k + 1 > t->kap) {
        uint32_t yk = t->kap ? t->kap : 4096;
        while (yk < t->n + k + 1) yk *= 2;
        if (yk > t->sinir) { t->tasti = 1; return 0; }
        uint8_t *y = (uint8_t *)up_ayir(yk);
        if (!y) { t->tasti = 1; return 0; }
        if (t->n) um_bellek_kopya(y, t->p, t->n);
        if (t->p) up_biraktir(t->p);
        t->p = y; t->kap = yk;
    }
    if (k) um_bellek_kopya(t->p + t->n, v, k);
    t->n += k;
    t->p[t->n] = 0;
    return 1;
}
static void tp_bayt(tampon_t *t, uint8_t b) { tp_ek(t, &b, 1); }
static void tp_birak(tampon_t *t) { if (t->p) up_biraktir(t->p); t->p = 0; t->n = t->kap = 0; }
static void tp_satir(tampon_t *t) {
    if (t->n == 0 || t->p[t->n - 1] == '\n') return;
    tp_bayt(t, '\n');
}
static void tp_boslukla(tampon_t *t) {
    if (t->n == 0) return;
    uint8_t c = t->p[t->n - 1];
    if (c == '\n' || c == ' ') return;
    tp_bayt(t, ' ');
}
static void tp_utf8(tampon_t *t, uint32_t kn) {
    if (kn == 0xAD || kn == 0) return;
    if (kn >= 0xFB00 && kn <= 0xFB04) {
        static const char *L[5] = { "ff", "fi", "fl", "ffi", "ffl" };
        const char *s = L[kn - 0xFB00]; tp_ek(t, s, um_uzun(s)); return;
    }
    char c[4]; uint32_t k = um_utf8_yaz(kn, c); tp_ek(t, c, k);
}

/* ---------------------------------------------------------------- inflate */
typedef struct { const uint8_t *g; uint32_t n, i; uint32_t bit; int nbit; int hata; } bitk_t;
typedef struct { uint16_t say[16]; uint16_t sem[320]; } huf_t;

static uint32_t bit_al(bitk_t *b, int k) {
    while (b->nbit < k) {
        if (b->i >= b->n) { b->hata = 1; return 0; }
        b->bit |= (uint32_t)b->g[b->i++] << b->nbit;
        b->nbit += 8;
    }
    uint32_t v = b->bit & ((1u << k) - 1u);
    b->bit >>= k; b->nbit -= k;
    return v;
}
static int huf_kur(huf_t *h, const uint8_t *uz, int n) {
    for (int i = 0; i < 16; i++) h->say[i] = 0;
    for (int i = 0; i < n; i++) h->say[uz[i]]++;
    if (h->say[0] == n) return 0;
    int sol = 1;
    for (int u = 1; u < 16; u++) { sol <<= 1; sol -= h->say[u]; if (sol < 0) return -1; }
    uint16_t off[16]; off[1] = 0;
    for (int u = 1; u < 15; u++) off[u + 1] = (uint16_t)(off[u] + h->say[u]);
    for (int i = 0; i < n; i++) if (uz[i]) h->sem[off[uz[i]]++] = (uint16_t)i;
    return sol;
}
static int huf_coz(bitk_t *b, const huf_t *h) {
    int kod = 0, ilk = 0, idx = 0;
    for (int u = 1; u < 16; u++) {
        kod |= (int)bit_al(b, 1);
        if (b->hata) return -1;
        int say = h->say[u];
        if (kod - say < ilk) return h->sem[idx + (kod - ilk)];
        idx += say; ilk += say; ilk <<= 1; kod <<= 1;
    }
    return -1;
}
static const uint16_t LUZ[29] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
static const uint16_t LEK[29] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
static const uint16_t DUZ[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
static const uint16_t DEK[30] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};

static int blok_coz(bitk_t *b, tampon_t *o, const huf_t *lh, const huf_t *dh) {
    for (;;) {
        int s = huf_coz(b, lh);
        if (s < 0) return -1;
        if (s < 256) { tp_bayt(o, (uint8_t)s); }
        else if (s == 256) return 0;
        else {
            s -= 257; if (s >= 29) return -1;
            uint32_t uz = LUZ[s] + bit_al(b, LEK[s]);
            int d = huf_coz(b, dh); if (d < 0 || d >= 30) return -1;
            uint32_t mes = DUZ[d] + bit_al(b, DEK[d]);
            if (b->hata || mes > o->n) return -1;
            for (uint32_t k = 0; k < uz; k++) tp_bayt(o, o->p[o->n - mes]);
        }
        if (o->tasti) return -2;
        if (b->hata) return -1;
    }
}

static int inflate_ham(const uint8_t *g, uint32_t n, tampon_t *o) {
    bitk_t b; b.g = g; b.n = n; b.i = 0; b.bit = 0; b.nbit = 0; b.hata = 0;
    huf_t *lh = (huf_t *)up_ayir(sizeof(huf_t)), *dh = (huf_t *)up_ayir(sizeof(huf_t));
    if (!lh || !dh) { if (lh) up_biraktir(lh); if (dh) up_biraktir(dh); return -1; }
    int son, r = 0;
    do {
        son = (int)bit_al(&b, 1);
        int tip = (int)bit_al(&b, 2);
        if (b.hata) { r = -1; break; }
        if (tip == 0) {
            b.bit = 0; b.nbit = 0;
            if (b.i + 4 > b.n) { r = -1; break; }
            uint32_t len = b.g[b.i] | (uint32_t)b.g[b.i + 1] << 8;
            b.i += 4;
            if (b.i + len > b.n) { r = -1; break; }
            tp_ek(o, b.g + b.i, len); b.i += len;
        } else if (tip == 1) {
            uint8_t uz[320]; int i = 0;
            for (; i < 144; i++) uz[i] = 8;
            for (; i < 256; i++) uz[i] = 9;
            for (; i < 280; i++) uz[i] = 7;
            for (; i < 288; i++) uz[i] = 8;
            huf_kur(lh, uz, 288);
            for (i = 0; i < 30; i++) uz[i] = 5;
            huf_kur(dh, uz, 30);
            r = blok_coz(&b, o, lh, dh);
        } else if (tip == 2) {
            uint8_t uz[320];
            int nl = (int)bit_al(&b, 5) + 257, nd = (int)bit_al(&b, 5) + 1, nc = (int)bit_al(&b, 4) + 4;
            if (b.hata || nl > 286 || nd > 30) { r = -1; break; }
            static const uint8_t SIRA[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
            int i;
            for (i = 0; i < 19; i++) uz[i] = 0;
            for (i = 0; i < nc; i++) uz[SIRA[i]] = (uint8_t)bit_al(&b, 3);
            huf_t *ch = (huf_t *)up_ayir(sizeof(huf_t));
            if (!ch) { r = -1; break; }
            if (huf_kur(ch, uz, 19) != 0) { up_biraktir(ch); r = -1; break; }
            int idx = 0;
            while (idx < nl + nd) {
                int sem = huf_coz(&b, ch);
                if (sem < 0) { r = -1; break; }
                if (sem < 16) uz[idx++] = (uint8_t)sem;
                else {
                    int tek = 0, rep;
                    if (sem == 16) { if (idx == 0) { r = -1; break; } tek = uz[idx - 1]; rep = 3 + (int)bit_al(&b, 2); }
                    else if (sem == 17) rep = 3 + (int)bit_al(&b, 3);
                    else rep = 11 + (int)bit_al(&b, 7);
                    if (idx + rep > nl + nd) { r = -1; break; }
                    while (rep--) uz[idx++] = (uint8_t)tek;
                }
            }
            up_biraktir(ch);
            if (r) break;
            huf_kur(lh, uz, nl);
            huf_kur(dh, uz + nl, nd);
            r = blok_coz(&b, o, lh, dh);
        } else { r = -1; break; }
        if (r) break;
    } while (!son);
    up_biraktir(lh); up_biraktir(dh);
    return r;
}

int pdf_inflate(const uint8_t *girdi, uint32_t n, uint8_t *cik, uint32_t cn) {
    if (n < 2) return -1;
    tampon_t o; o.p = 0; o.n = o.kap = 0; o.tasti = 0; o.sinir = PDF_AKIS_MAX;
    int r = inflate_ham(girdi + 2, n - 2, &o);       /* zlib başlığını atla */
    int sonuc = r;
    if (r == 0) {
        if (o.n > cn) sonuc = -1;
        else { um_bellek_kopya(cik, o.p, o.n); sonuc = (int)o.n; }
    }
    tp_birak(&o);
    return sonuc;
}

/* ---------------------------------------------------------------- PDF nesne ayrıştırma */
typedef struct { const uint8_t *p; uint32_t n; } iz_t;      /* [p, p+n) */

static int bel_esit(const void *a, const void *b, uint32_t n) {
    const uint8_t *x = (const uint8_t *)a, *y = (const uint8_t *)b;
    for (uint32_t i = 0; i < n; i++) if (x[i] != y[i]) return 0;
    return 1;
}
static int bosluk(uint8_t c) { return c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\f' || c == 0; }
static int ayrac(uint8_t c) {
    return c == '(' || c == ')' || c == '<' || c == '>' || c == '[' || c == ']' || c == '{' || c == '}' || c == '/' || c == '%';
}
static const uint8_t *bos_atla(const uint8_t *p, const uint8_t *s) {
    while (p < s) {
        if (bosluk(*p)) p++;
        else if (*p == '%') { while (p < s && *p != '\n' && *p != '\r') p++; }
        else break;
    }
    return p;
}
static int ara_bellek(const uint8_t *h, uint32_t hn, const char *iz, uint32_t *konum) {
    uint32_t k = um_uzun(iz);
    if (k == 0 || hn < k) return 0;
    for (uint32_t i = 0; i + k <= hn; i++) {
        if (h[i] == (uint8_t)iz[0] && bel_esit(h + i, iz, k)) { *konum = i; return 1; }
    }
    return 0;
}

/* Tek bir PDF değerinin sonunu bulur (başlangıçta boşluk yok). */
static const uint8_t *deger_atla(const uint8_t *p, const uint8_t *s, int derin) {
    if (p >= s || derin > 40) return s;
    uint8_t c = *p;
    if (c == '(') {
        int d = 0;
        for (; p < s; p++) {
            if (*p == '\\') { p++; continue; }
            if (*p == '(') d++;
            else if (*p == ')') { d--; if (d == 0) return p + 1; }
        }
        return s;
    }
    if (c == '<') {
        if (p + 1 < s && p[1] == '<') {
            p += 2;
            for (;;) {
                p = bos_atla(p, s);
                if (p >= s) return s;
                if (p + 1 < s && p[0] == '>' && p[1] == '>') return p + 2;
                p = deger_atla(p, s, derin + 1);
            }
        }
        while (p < s && *p != '>') p++;
        return p < s ? p + 1 : s;
    }
    if (c == '[') {
        p++;
        for (;;) {
            p = bos_atla(p, s);
            if (p >= s) return s;
            if (*p == ']') return p + 1;
            p = deger_atla(p, s, derin + 1);
        }
    }
    if (c == '/') { p++; while (p < s && !bosluk(*p) && !ayrac(*p)) p++; return p; }
    if (c == ')' || c == '>' || c == ']' || c == '{' || c == '}') return p + 1;
    while (p < s && !bosluk(*p) && !ayrac(*p)) p++;
    return p;
}

static int sayi_mi(const uint8_t *p, const uint8_t *s) {
    if (p >= s) return 0;
    return (*p >= '0' && *p <= '9');
}
static int64_t tam_oku(const uint8_t *p, const uint8_t *s, const uint8_t **son) {
    int neg = 0; int64_t v = 0;
    if (p < s && (*p == '-' || *p == '+')) { neg = *p == '-'; p++; }
    while (p < s && *p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; if (v > 0x7FFFFFFFFFLL) v = 0x7FFFFFFFFFLL; }
    if (son) *son = p;
    return neg ? -v : v;
}

/* Değer bir dolaylı başvuru ise ("N G R") numarayı döner, değilse -1. */
static int64_t basvuru(iz_t d) {
    const uint8_t *p = d.p, *s = d.p + d.n, *q;
    if (!sayi_mi(p, s)) return -1;
    int64_t num = tam_oku(p, s, &q);
    q = bos_atla(q, s);
    if (!sayi_mi(q, s)) return -1;
    tam_oku(q, s, &q);
    q = bos_atla(q, s);
    if (q < s && *q == 'R' && (q + 1 >= s || bosluk(q[1]) || ayrac(q[1]))) return num;
    return -1;
}

/* Sözlük içinde /Anahtar değerini bulur. d: "<<...>>" ya da içeriği. */
static int sozluk_al(iz_t d, const char *anahtar, iz_t *deger) {
    const uint8_t *p = d.p, *s = d.p + d.n;
    p = bos_atla(p, s);
    if (p + 1 < s && p[0] == '<' && p[1] == '<') p += 2;
    uint32_t ak = um_uzun(anahtar);
    for (;;) {
        p = bos_atla(p, s);
        if (p >= s || (p + 1 < s && p[0] == '>' && p[1] == '>')) return 0;
        if (*p != '/') { p = deger_atla(p, s, 0); continue; }
        const uint8_t *ad = p + 1;
        const uint8_t *adson = deger_atla(p, s, 0);
        uint32_t al = (uint32_t)(adson - ad);
        p = bos_atla(adson, s);
        const uint8_t *v0 = p;
        const uint8_t *v1 = deger_atla(p, s, 0);
        /* başvuru: "N G R" */
        if (sayi_mi(v0, s)) {
            const uint8_t *q = bos_atla(v1, s), *q2;
            if (sayi_mi(q, s)) {
                q2 = deger_atla(q, s, 0);
                const uint8_t *q3 = bos_atla(q2, s);
                if (q3 < s && *q3 == 'R' && (q3 + 1 >= s || bosluk(q3[1]) || ayrac(q3[1]))) v1 = q3 + 1;
            }
        }
        if (al == ak - 1 && bel_esit(ad, anahtar + 1, al)) {
            deger->p = v0; deger->n = (uint32_t)(v1 - v0);
            return 1;
        }
        p = v1;
    }
}

/* ---------------------------------------------------------------- nesne tablosu */
typedef struct { const uint8_t *g; uint32_t n; } nesne_t;   /* gövde: obj ... endobj arası */

typedef struct {
    const uint8_t *pdf; uint32_t pn;
    nesne_t *tab;                 /* PDF_NESNE_MAX */
    uint8_t **tut; int tut_say;   /* ObjStm gibi çözülmüş tamponlar */
    int tut_kap;
    int akis_hata;                /* desteklenmeyen süzgeç görüldü */
    int bilinmeyen_yazi;
} pdf_t;

static void tut_ekle(pdf_t *d, uint8_t *p) {
    if (d->tut_say >= d->tut_kap) {
        int yk = d->tut_kap ? d->tut_kap * 2 : 64;
        uint8_t **y = (uint8_t **)up_ayir((uint64_t)yk * sizeof(uint8_t *));
        if (!y) { up_biraktir(p); return; }
        if (d->tut_say) um_bellek_kopya(y, d->tut, (uint64_t)d->tut_say * sizeof(uint8_t *));
        if (d->tut) up_biraktir(d->tut);
        d->tut = y; d->tut_kap = yk;
    }
    d->tut[d->tut_say++] = p;
}

static int govde_sozluk(nesne_t o, iz_t *sz) {
    const uint8_t *p = bos_atla(o.g, o.g + o.n), *s = o.g + o.n;
    if (p + 1 < s && p[0] == '<' && p[1] == '<') {
        const uint8_t *e = deger_atla(p, s, 0);
        sz->p = p; sz->n = (uint32_t)(e - p); return 1;
    }
    return 0;
}

static int nesne_al(pdf_t *d, int64_t num, nesne_t *o) {
    if (num < 0 || num >= (int64_t)PDF_NESNE_MAX || !d->tab[num].g) return 0;
    *o = d->tab[num]; return 1;
}

/* Bir değeri (başvuru olabilir) sözlük ya da dizi/değer iziyle çözer. */
static iz_t coz(pdf_t *d, iz_t v) {
    for (int k = 0; k < 8; k++) {
        int64_t r = basvuru(v);
        if (r < 0) return v;
        nesne_t o;
        if (!nesne_al(d, r, &o)) { v.n = 0; return v; }
        const uint8_t *p = bos_atla(o.g, o.g + o.n);
        const uint8_t *e = deger_atla(p, o.g + o.n, 0);
        v.p = p; v.n = (uint32_t)(e - p);
    }
    return v;
}

/* Akışın çözülmüş baytları (öbekte); çağıran up_biraktir eder. */
static int hex_say(uint8_t c);
static int akis_coz(pdf_t *d, nesne_t o, tampon_t *cik) {
    iz_t sz;
    if (!govde_sozluk(o, &sz)) return 0;
    const uint8_t *s = o.g + o.n;
    const uint8_t *p = sz.p + sz.n;
    p = bos_atla(p, s);
    if (!(p + 6 <= s && bel_esit(p, "stream", 6))) return 0;
    p += 6;
    if (p < s && *p == '\r') p++;
    if (p < s && *p == '\n') p++;
    uint32_t uz = 0; int uz_var = 0;
    iz_t lv;
    if (sozluk_al(sz, "/Length", &lv)) {
        iz_t lc = coz(d, lv);
        const uint8_t *q;
        if (sayi_mi(lc.p, lc.p + lc.n)) { int64_t L = tam_oku(lc.p, lc.p + lc.n, &q); if (L >= 0 && (uint64_t)L <= (uint64_t)(s - p)) { uz = (uint32_t)L; uz_var = 1; } }
    }
    if (uz_var) {                         /* endstream gerçekten orada mı? */
        const uint8_t *q = bos_atla(p + uz, s);
        if (!(q + 9 <= s && bel_esit(q, "endstream", 9))) uz_var = 0;
    }
    if (!uz_var) {
        uint32_t k;
        if (!ara_bellek(p, (uint32_t)(s - p), "endstream", &k)) k = (uint32_t)(s - p);
        uz = k;
        while (uz && (p[uz - 1] == '\n' || p[uz - 1] == '\r')) uz--;
    }
    iz_t fv;
    int zincir[6], zn = 0, baska = 0;          /* 1=Flate 2=A85 3=AHex */
    if (sozluk_al(sz, "/Filter", &fv)) {
        iz_t f = coz(d, fv);
        const uint8_t *q = f.p, *fe = f.p + f.n;
        while (q < fe) {
            if (*q == '/') {
                const uint8_t *e = q + 1; while (e < fe && !bosluk(*e) && !ayrac(*e)) e++;
                uint32_t L = (uint32_t)(e - q - 1);
                if (L >= 2 && bel_esit(q + 1, "Fl", 2)) { if (zn < 6) zincir[zn++] = 1; }
                else if (L >= 2 && bel_esit(q + 1, "A85", 3)) { if (zn < 6) zincir[zn++] = 2; }
                else if (L >= 3 && bel_esit(q + 1, "ASCII85", 7)) { if (zn < 6) zincir[zn++] = 2; }
                else if (L >= 3 && (bel_esit(q + 1, "AHx", 3) || bel_esit(q + 1, "ASCIIHex", 8))) { if (zn < 6) zincir[zn++] = 3; }
                else baska = 1;                 /* LZW, DCT, JPX, CCITT, Crypt, RunLength ... */
                q = e;
            } else q++;
        }
    }
    if (baska) { d->akis_hata = 1; return 0; }
    iz_t dp;
    if (sozluk_al(sz, "/DecodeParms", &dp)) {
        iz_t pr;
        iz_t dpc = coz(d, dp);
        if (sozluk_al(dpc, "/Predictor", &pr)) {
            const uint8_t *q; int64_t pv = tam_oku(pr.p, pr.p + pr.n, &q);
            if (pv >= 10) { d->akis_hata = 1; return 0; }     /* PNG öngörücü: içerik akışlarında beklenmez */
        }
    }
    cik->p = 0; cik->n = cik->kap = 0; cik->tasti = 0; cik->sinir = PDF_AKIS_MAX;
    tp_ek(cik, p, uz);
    for (int zi = 0; zi < zn; zi++) {
        tampon_t y; y.p = 0; y.n = y.kap = 0; y.tasti = 0; y.sinir = PDF_AKIS_MAX;
        if (zincir[zi] == 1) {
            if (cik->n >= 2) inflate_ham(cik->p + 2, cik->n - 2, &y);   /* kısmî çıktı da işe yarar */
        } else if (zincir[zi] == 2) {
            uint32_t grp = 0; int say = 0;
            for (uint32_t i = 0; i < cik->n; i++) {
                uint8_t c = cik->p[i];
                if (bosluk(c)) continue;
                if (c == '~') break;
                if (c == 'z' && say == 0) { tp_bayt(&y, 0); tp_bayt(&y, 0); tp_bayt(&y, 0); tp_bayt(&y, 0); continue; }
                if (c < '!' || c > 'u') continue;
                grp = grp * 85 + (uint32_t)(c - '!'); say++;
                if (say == 5) { tp_bayt(&y, (uint8_t)(grp >> 24)); tp_bayt(&y, (uint8_t)(grp >> 16)); tp_bayt(&y, (uint8_t)(grp >> 8)); tp_bayt(&y, (uint8_t)grp); grp = 0; say = 0; }
            }
            if (say > 1) {
                for (int k = say; k < 5; k++) grp = grp * 85 + 84;
                for (int k = 0; k < say - 1; k++) tp_bayt(&y, (uint8_t)(grp >> (24 - 8 * k)));
            }
        } else {
            int yarim = -1;
            for (uint32_t i = 0; i < cik->n; i++) {
                if (cik->p[i] == '>') break;
                int h = hex_say(cik->p[i]);
                if (h < 0) continue;
                if (yarim < 0) yarim = h; else { tp_bayt(&y, (uint8_t)(yarim * 16 + h)); yarim = -1; }
            }
            if (yarim >= 0) tp_bayt(&y, (uint8_t)(yarim * 16));
        }
        tp_birak(cik);
        *cik = y;
        if (cik->n == 0) { tp_birak(cik); return 0; }
    }
    return cik->p != 0;
}

static void nesneleri_tara(pdf_t *d) {
    const uint8_t *g = d->pdf; uint32_t n = d->pn;
    uint32_t i = 0;
    while (i + 3 < n) {
        if (g[i] == 'o' && g[i + 1] == 'b' && g[i + 2] == 'j' && i > 2 && (g[i + 3] == 0 || bosluk(g[i + 3]) || g[i + 3] == '<' || g[i + 3] == '[' || g[i + 3] == '(' || g[i + 3] == '/' || (g[i + 3] >= '0' && g[i + 3] <= '9') || g[i + 3] == '-')) {
            int64_t j = (int64_t)i - 1;
            while (j >= 0 && bosluk(g[j])) j--;
            int64_t ge = j;
            while (j >= 0 && g[j] >= '0' && g[j] <= '9') j--;
            if (j < ge && j >= 0 && bosluk(g[j])) {
                while (j >= 0 && bosluk(g[j])) j--;
                int64_t ne = j;
                while (j >= 0 && g[j] >= '0' && g[j] <= '9') j--;
                if (j < ne) {
                    const uint8_t *q;
                    int64_t num = tam_oku(g + j + 1, g + ne + 1, &q);
                    uint32_t bas = i + 3, e;
                    /* gövde: "endobj"a kadar; akış varsa endstream sonrası */
                    const uint8_t *bp = bos_atla(g + bas, g + n);
                    uint32_t bo = (uint32_t)(bp - g);
                    uint32_t k;
                    uint32_t aramaa = bo;
                    /* sözlükten sonra "stream" geliyorsa akışın sonuna atla */
                    if (bp + 1 < g + n && bp[0] == '<' && bp[1] == '<') {
                        const uint8_t *de = deger_atla(bp, g + n, 0);
                        const uint8_t *sp = bos_atla(de, g + n);
                        if (sp + 6 <= g + n && bel_esit(sp, "stream", 6)) {
                            uint32_t kk;
                            if (ara_bellek(sp, (uint32_t)(g + n - sp), "endstream", &kk)) aramaa = (uint32_t)(sp - g) + kk + 9;
                        }
                    }
                    if (ara_bellek(g + aramaa, n - aramaa, "endobj", &k)) e = aramaa + k; else e = n;
                    if (num >= 0 && num < (int64_t)PDF_NESNE_MAX) {
                        d->tab[num].g = g + bas; d->tab[num].n = e - bas;
                    }
                    i = e + 6 < n ? e + 6 : n;
                    continue;
                }
            }
        }
        i++;
    }
}

/* Nesne akışlarındaki (ObjStm) nesneleri tabloya ekler. */
static void nesne_akislari(pdf_t *d) {
    for (uint32_t num = 0; num < PDF_NESNE_MAX; num++) {
        nesne_t o = d->tab[num];
        if (!o.g) continue;
        iz_t sz;
        if (!govde_sozluk(o, &sz)) continue;
        uint32_t k;
        if (!ara_bellek(sz.p, sz.n, "/ObjStm", &k)) continue;
        iz_t nv, fv;
        if (!sozluk_al(sz, "/N", &nv) || !sozluk_al(sz, "/First", &fv)) continue;
        const uint8_t *q;
        int64_t N = tam_oku(nv.p, nv.p + nv.n, &q), F = tam_oku(fv.p, fv.p + fv.n, &q);
        tampon_t t;
        if (!akis_coz(d, o, &t)) continue;
        uint8_t *bf = t.p; uint32_t bn = t.n;
        tut_ekle(d, bf);
        const uint8_t *p = bf, *s = bf + bn;
        /* N çift: (nesne numarası, ofset) */
        int64_t no[1024]; int64_t of[1024];
        int say = N > 1024 ? 1024 : (int)N;
        for (int a = 0; a < say; a++) {
            p = bos_atla(p, s); no[a] = tam_oku(p, s, &q); p = q;
            p = bos_atla(p, s); of[a] = tam_oku(p, s, &q); p = q;
        }
        for (int a = 0; a < say; a++) {
            int64_t b0 = F + of[a];
            int64_t b1 = (a + 1 < say) ? F + of[a + 1] : (int64_t)bn;
            if (b0 < 0 || b0 >= (int64_t)bn || b1 > (int64_t)bn || b1 < b0) continue;
            if (no[a] >= 0 && no[a] < (int64_t)PDF_NESNE_MAX) {
                if (d->tab[no[a]].g == 0 || 1) { d->tab[no[a]].g = bf + b0; d->tab[no[a]].n = (uint32_t)(b1 - b0); }
            }
        }
    }
}

/* ---------------------------------------------------------------- ToUnicode */
typedef struct { uint32_t lo, hi, d1, d2; } cm_t;
typedef struct {
    int64_t nesne;
    int iki_bayt;
    cm_t *e; int say, kap; int var;
} yazi_t;

static void cm_ekle(yazi_t *y, uint32_t lo, uint32_t hi, uint32_t d1, uint32_t d2) {
    if (y->say >= y->kap) {
        int yk = y->kap ? y->kap * 2 : 256;
        if (yk > 65536) return;
        cm_t *z = (cm_t *)up_ayir((uint64_t)yk * sizeof(cm_t));
        if (!z) return;
        if (y->say) um_bellek_kopya(z, y->e, (uint64_t)y->say * sizeof(cm_t));
        if (y->e) up_biraktir(y->e);
        y->e = z; y->kap = yk;
    }
    y->e[y->say].lo = lo; y->e[y->say].hi = hi; y->e[y->say].d1 = d1; y->e[y->say].d2 = d2;
    y->say++;
}
static int hex_say(uint8_t c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
/* <...> onaltılı dizgeyi okur; değerleri 16'şar bitlik birim olarak ya da ham baytlar olarak döner */
static const uint8_t *hex_oku(const uint8_t *p, const uint8_t *s, uint8_t *bay, int *bn) {
    *bn = 0;
    if (p >= s || *p != '<') return 0;
    p++;
    int yarim = -1;
    while (p < s && *p != '>') {
        int h = hex_say(*p);
        if (h >= 0) {
            if (yarim < 0) yarim = h;
            else { if (*bn < 32) bay[(*bn)++] = (uint8_t)(yarim * 16 + h); yarim = -1; }
        }
        p++;
    }
    if (yarim >= 0 && *bn < 32) bay[(*bn)++] = (uint8_t)(yarim * 16);
    return p < s ? p + 1 : s;
}
static uint32_t bay_say(const uint8_t *b, int n) {
    uint32_t v = 0; for (int i = 0; i < n && i < 4; i++) v = (v << 8) | b[i]; return v;
}
static void utf16_coz(const uint8_t *b, int n, uint32_t *d1, uint32_t *d2) {
    *d1 = 0; *d2 = 0;
    if (n == 1) { *d1 = b[0]; return; }
    uint32_t u0 = ((uint32_t)b[0] << 8) | b[1];
    int k = 2;
    if (u0 >= 0xD800 && u0 < 0xDC00 && n >= 4) { uint32_t u1 = ((uint32_t)b[2] << 8) | b[3]; *d1 = 0x10000 + ((u0 - 0xD800) << 10) + (u1 - 0xDC00); k = 4; }
    else *d1 = u0;
    if (n >= k + 2) *d2 = ((uint32_t)b[k] << 8) | b[k + 1];
}

static void cm_sirala(yazi_t *y) {
    for (int gap = y->say / 2; gap > 0; gap /= 2)
        for (int i = gap; i < y->say; i++) {
            cm_t t = y->e[i]; int j = i;
            while (j >= gap && y->e[j - gap].lo > t.lo) { y->e[j] = y->e[j - gap]; j -= gap; }
            y->e[j] = t;
        }
}

static void cmap_oku(const uint8_t *b, uint32_t bn, yazi_t *y) {
    const uint8_t *p = b, *s = b + bn;
    uint8_t a1[32], a2[32], a3[32]; int n1, n2, n3;
    while (p < s) {
        uint32_t k;
        int bc = ara_bellek(p, (uint32_t)(s - p), "beginbfchar", &k);
        uint32_t k2;
        int br = ara_bellek(p, (uint32_t)(s - p), "beginbfrange", &k2);
        if (!bc && !br) break;
        int ch = bc && (!br || k < k2);
        p += ch ? k + 11 : k2 + 12;
        for (;;) {
            p = bos_atla(p, s);
            if (p >= s || *p != '<') break;
            p = hex_oku(p, s, a1, &n1);
            p = bos_atla(p, s);
            if (!p || p >= s) break;
            if (ch) {
                if (*p != '<') break;
                p = hex_oku(p, s, a2, &n2);
                if (n1 >= 1 && n2 >= 1) { uint32_t d1, d2; utf16_coz(a2, n2, &d1, &d2); uint32_t c = bay_say(a1, n1); cm_ekle(y, c, c, d1, d2); }
                if (n1 >= 2) y->iki_bayt = 1;
            } else {
                if (*p != '<') break;
                p = hex_oku(p, s, a2, &n2);
                p = bos_atla(p, s);
                if (!p || p >= s) break;
                uint32_t lo = bay_say(a1, n1), hi = bay_say(a2, n2);
                if (n1 >= 2) y->iki_bayt = 1;
                if (*p == '<') {
                    p = hex_oku(p, s, a3, &n3);
                    uint32_t d1, d2; utf16_coz(a3, n3, &d1, &d2);
                    if (hi >= lo && hi - lo < 65536) cm_ekle(y, lo, hi, d1, d2);
                } else if (*p == '[') {
                    p++;
                    for (uint32_t c = lo; ; c++) {
                        p = bos_atla(p, s);
                        if (p >= s || *p == ']') break;
                        p = hex_oku(p, s, a3, &n3);
                        if (!p) break;
                        uint32_t d1, d2; utf16_coz(a3, n3, &d1, &d2);
                        if (c <= hi) cm_ekle(y, c, c, d1, d2);
                    }
                    if (p < s && *p == ']') p++;
                } else break;
            }
        }
    }
    cm_sirala(y);
    y->var = y->say > 0;
}

static int cm_bul(const yazi_t *y, uint32_t kod, uint32_t *d1, uint32_t *d2) {
    int a = 0, b = y->say - 1, en = -1;
    while (a <= b) {
        int o = (a + b) / 2;
        if (y->e[o].lo <= kod) { en = o; a = o + 1; } else b = o - 1;
    }
    for (int i = en; i >= 0 && i > en - 4; i--) {
        if (kod >= y->e[i].lo && kod <= y->e[i].hi) {
            *d1 = y->e[i].d1 + (kod - y->e[i].lo);
            *d2 = y->e[i].d2;
            return 1;
        }
    }
    return 0;
}

static uint32_t cp1252(uint8_t c) {
    static const uint16_t U[32] = { 0x20AC,0x81,0x201A,0x192,0x201E,0x2026,0x2020,0x2021,0x2C6,0x2030,0x160,0x2039,0x152,0x8D,0x17D,0x8F,
                                    0x90,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014,0x2DC,0x2122,0x161,0x203A,0x153,0x9D,0x17E,0x178 };
    if (c >= 0x80 && c < 0xA0) return U[c - 0x80];
    return c;
}

/* ---------------------------------------------------------------- içerik akışı yorumlayıcı */
typedef struct {
    pdf_t *d;
    iz_t font_sz;                 /* Resources /Font sözlüğü */
    yazi_t *yazilar; int yazi_say;
    yazi_t *gecerli;
    tampon_t *cikti;
    int64_t son_y; int y_var;
} yor_t;

static yazi_t *yazi_bul(yor_t *y, const uint8_t *ad, uint32_t al) {
    iz_t kz, v;
    char anah[80];
    if (al == 0 || al > 70) return 0;
    anah[0] = '/'; um_bellek_kopya(anah + 1, ad, al); anah[al + 1] = 0;
    if (!y->font_sz.n) return 0;
    kz = coz(y->d, y->font_sz);
    if (!sozluk_al(kz, anah, &v)) return 0;
    int64_t r = basvuru(v);
    for (int i = 0; i < y->yazi_say; i++) if (r >= 0 && y->yazilar[i].nesne == r) return &y->yazilar[i];
    if (y->yazi_say >= PDF_YAZI_MAX) return 0;
    yazi_t *yz = &y->yazilar[y->yazi_say++];
    um_bellek_doldur(yz, 0, sizeof *yz);
    yz->nesne = r;
    iz_t fd = coz(y->d, v);
    iz_t tv;
    uint32_t k;
    if (sozluk_al(fd, "/Subtype", &tv) && ara_bellek(tv.p, tv.n, "Type0", &k)) yz->iki_bayt = 1;
    if (sozluk_al(fd, "/ToUnicode", &tv)) {
        int64_t tr = basvuru(tv);
        nesne_t o;
        if (tr >= 0 && nesne_al(y->d, tr, &o)) {
            tampon_t t;
            if (akis_coz(y->d, o, &t)) { cmap_oku(t.p, t.n, yz); tp_birak(&t); }
        }
    }
    return yz;
}

static void metin_cikar(yor_t *y, const uint8_t *s, uint32_t n, tampon_t *tmp) {
    yazi_t *f = y->gecerli;
    uint32_t i = 0;
    int ikili = f ? f->iki_bayt : 0;
    if (f && f->var && f->iki_bayt == 0) {
        /* ToUnicode kodları 2 baytlıysa (ör. TrueType alt kümesi) 2 bayt oku */
        for (int a = 0; a < f->say && a < 8; a++) if (f->e[a].hi > 255) { ikili = 1; break; }
    }
    while (i < n) {
        uint32_t kod, d1 = 0, d2 = 0;
        if (ikili) { if (i + 1 >= n) { kod = s[i]; i++; } else { kod = ((uint32_t)s[i] << 8) | s[i + 1]; i += 2; } }
        else { kod = s[i]; i++; }
        if (f && f->var) {
            if (cm_bul(f, kod, &d1, &d2)) { tp_utf8(tmp, d1); if (d2) tp_utf8(tmp, d2); }
            else if (!ikili && kod < 0x20) { }
            else if (!ikili) tp_utf8(tmp, cp1252((uint8_t)kod));
            else y->d->bilinmeyen_yazi++;
        } else if (ikili) {
            y->d->bilinmeyen_yazi++;
        } else {
            tp_utf8(tmp, cp1252((uint8_t)kod));
        }
    }
}

/* Değişmez ya da onaltılı dizgeyi ham bayta çevirir. */
static const uint8_t *dizge_oku(const uint8_t *p, const uint8_t *s, tampon_t *hb) {
    hb->n = 0;
    if (*p == '(') {
        int d = 0;
        for (; p < s; p++) {
            uint8_t c = *p;
            if (c == '\\') {
                p++; if (p >= s) break;
                c = *p;
                if (c == 'n') c = '\n'; else if (c == 'r') c = '\r'; else if (c == 't') c = '\t';
                else if (c == 'b') c = '\b'; else if (c == 'f') c = '\f';
                else if (c >= '0' && c <= '7') {
                    int v = c - '0';
                    for (int k = 0; k < 2 && p + 1 < s && p[1] >= '0' && p[1] <= '7'; k++) { p++; v = v * 8 + (*p - '0'); }
                    c = (uint8_t)v;
                } else if (c == '\n') continue;
                else if (c == '\r') { if (p + 1 < s && p[1] == '\n') p++; continue; }
                tp_bayt(hb, c);
                continue;
            }
            if (c == '(') { d++; if (d == 1) continue; }
            else if (c == ')') { d--; if (d == 0) return p + 1; }
            tp_bayt(hb, c);
        }
        return s;
    }
    /* < > */
    p++;
    int yarim = -1;
    while (p < s && *p != '>') {
        int h = hex_say(*p);
        if (h >= 0) { if (yarim < 0) yarim = h; else { tp_bayt(hb, (uint8_t)(yarim * 16 + h)); yarim = -1; } }
        p++;
    }
    if (yarim >= 0) tp_bayt(hb, (uint8_t)(yarim * 16));
    return p < s ? p + 1 : s;
}

static int64_t sayi_bin(const uint8_t *p, const uint8_t *s) {
    int neg = 0; int64_t v = 0, kes = 0; int kd = 0, nokta = 0;
    if (p < s && (*p == '-' || *p == '+')) { neg = *p == '-'; p++; }
    for (; p < s; p++) {
        if (*p == '.') { nokta = 1; continue; }
        if (*p < '0' || *p > '9') break;
        if (!nokta) { v = v * 10 + (*p - '0'); if (v > 1000000000LL) v = 1000000000LL; }
        else if (kd < 3) { kes = kes * 10 + (*p - '0'); kd++; }
    }
    while (kd < 3) { kes *= 10; kd++; }
    int64_t r = v * 1000 + kes;
    return neg ? -r : r;
}

static void icerik_yorumla(yor_t *y, const uint8_t *g, uint32_t n) {
    const uint8_t *p = g, *s = g + n;
    tampon_t bekle; bekle.p = 0; bekle.n = bekle.kap = 0; bekle.tasti = 0; bekle.sinir = 1u << 20;
    tampon_t hb;    hb.p = 0;    hb.n = hb.kap = 0;       hb.tasti = 0;    hb.sinir = 1u << 20;
    int64_t sa[8]; int sn = 0;
    const uint8_t *son_ad = 0; uint32_t son_ad_n = 0;
    int bekle_var = 0;
    y->gecerli = 0;
    while (p < s) {
        p = bos_atla(p, s);
        if (p >= s) break;
        uint8_t c = *p;
        if (c == '(' || (c == '<' && !(p + 1 < s && p[1] == '<'))) {
            p = dizge_oku(p, s, &hb);
            bekle.n = 0; if (bekle.p) bekle.p[0] = 0;
            metin_cikar(y, hb.p ? hb.p : (const uint8_t *)"", hb.n, &bekle);
            bekle_var = 1;
        } else if (c == '[') {
            p++;
            bekle.n = 0; if (bekle.p) bekle.p[0] = 0;
            for (;;) {
                p = bos_atla(p, s);
                if (p >= s || *p == ']') { if (p < s) p++; break; }
                if (*p == '(' || (*p == '<' && !(p + 1 < s && p[1] == '<'))) {
                    p = dizge_oku(p, s, &hb);
                    metin_cikar(y, hb.p ? hb.p : (const uint8_t *)"", hb.n, &bekle);
                } else if ((*p >= '0' && *p <= '9') || *p == '-' || *p == '.' || *p == '+') {
                    const uint8_t *q = p; while (q < s && !bosluk(*q) && !ayrac(*q)) q++;
                    int64_t kv = sayi_bin(p, q);
                    if (kv <= -200000) tp_bayt(&bekle, ' ');
                    p = q;
                } else p = deger_atla(p, s, 0);
            }
            bekle_var = 1;
        } else if (c == '<') {                    /* << sözlük >> */
            p = deger_atla(p, s, 0);
        } else if (c == '/') {
            const uint8_t *q = p + 1; while (q < s && !bosluk(*q) && !ayrac(*q)) q++;
            son_ad = p + 1; son_ad_n = (uint32_t)(q - p - 1);
            p = q;
        } else if ((c >= '0' && c <= '9') || c == '-' || c == '.' || c == '+') {
            const uint8_t *q = p; while (q < s && !bosluk(*q) && !ayrac(*q)) q++;
            if (sn == 8) { for (int k = 1; k < 8; k++) sa[k - 1] = sa[k]; sn = 7; }
            sa[sn++] = sayi_bin(p, q);
            p = q;
        } else if (c == ']' || c == ')' || c == '>' || c == '{' || c == '}') {
            p++;
        } else {
            const uint8_t *q = p; while (q < s && !bosluk(*q) && !ayrac(*q)) q++;
            uint32_t ol = (uint32_t)(q - p);
            const uint8_t *o = p;
            p = q;
            if (ol == 2 && o[0] == 'T' && o[1] == 'f') {
                yazi_t *f = son_ad ? yazi_bul(y, son_ad, son_ad_n) : 0;
                y->gecerli = f;
            } else if ((ol == 2 && o[0] == 'T' && (o[1] == 'j' || o[1] == 'J')) ) {
                if (bekle_var) { tp_ek(y->cikti, bekle.p ? bekle.p : (const uint8_t *)"", bekle.n); }
            } else if (ol == 1 && (o[0] == '\'' || o[0] == '"')) {
                tp_satir(y->cikti);
                if (bekle_var) tp_ek(y->cikti, bekle.p ? bekle.p : (const uint8_t *)"", bekle.n);
            } else if (ol == 2 && o[0] == 'T' && (o[1] == 'd' || o[1] == 'D')) {
                if (sn >= 2 && sa[sn - 1] != 0) tp_satir(y->cikti); else tp_boslukla(y->cikti);
            } else if (ol == 2 && o[0] == 'T' && o[1] == '*') {
                tp_satir(y->cikti);
            } else if (ol == 2 && o[0] == 'T' && o[1] == 'm') {
                int64_t yy = sn >= 2 ? sa[sn - 1] : 0;
                if (y->y_var && yy != y->son_y) tp_satir(y->cikti); else if (y->y_var) tp_boslukla(y->cikti);
                y->son_y = yy; y->y_var = 1;
            } else if (ol == 2 && o[0] == 'E' && o[1] == 'T') {
                tp_satir(y->cikti);
            } else if (ol == 2 && o[0] == 'B' && o[1] == 'I') {
                uint32_t k;      /* satır içi resim: EI'ye kadar atla */
                if (ara_bellek(p, (uint32_t)(s - p), "\nEI", &k) || ara_bellek(p, (uint32_t)(s - p), " EI", &k)) p += k + 3; else p = s;
            }
            sn = 0; bekle_var = 0; son_ad = 0;
        }
    }
    tp_birak(&bekle); tp_birak(&hb);
}

/* ---------------------------------------------------------------- sayfa ağacı */
typedef struct { pdf_t *d; tampon_t *cikti; int sayfa; yazi_t *yazilar; int ziyaret; } gez_t;

static void sayfa_isle(gez_t *g, iz_t sayfa_sz, iz_t kaynak) {
    pdf_t *d = g->d;
    iz_t cv;
    if (!sozluk_al(sayfa_sz, "/Contents", &cv)) return;
    yor_t y; um_bellek_doldur(&y, 0, sizeof y);
    y.d = d; y.cikti = g->cikti; y.yazilar = g->yazilar;
    y.yazi_say = 0;
    iz_t kz = coz(d, kaynak);
    iz_t fv;
    if (kz.n && sozluk_al(kz, "/Font", &fv)) y.font_sz = fv;
    /* yazı tablosu sayfa başına sıfırlanır (bellek sızıntısını önlemek için eskileri bırak) */
    iz_t cc = coz(d, cv);
    iz_t liste[64]; int ls = 0;
    int64_t r0 = basvuru(cv);
    if (r0 >= 0 && cc.n && cc.p[0] != '[') { liste[ls++] = cv; }
    else if (cc.n && cc.p[0] == '[') {
        const uint8_t *p = cc.p + 1, *s = cc.p + cc.n;
        while (ls < 64) {
            p = bos_atla(p, s);
            if (p >= s || *p == ']') break;
            const uint8_t *q = deger_atla(p, s, 0);
            const uint8_t *q2 = bos_atla(q, s);
            const uint8_t *q3 = deger_atla(q2, s, 0);
            const uint8_t *q4 = bos_atla(q3, s);
            if (q4 < s && *q4 == 'R') { liste[ls].p = p; liste[ls].n = (uint32_t)(q4 + 1 - p); ls++; p = q4 + 1; }
            else break;
        }
    }
    tampon_t tum; tum.p = 0; tum.n = tum.kap = 0; tum.tasti = 0; tum.sinir = PDF_AKIS_MAX;
    for (int i = 0; i < ls; i++) {
        int64_t r = basvuru(liste[i]);
        nesne_t o; tampon_t t;
        if (r < 0 || !nesne_al(d, r, &o)) continue;
        if (akis_coz(d, o, &t)) { tp_ek(&tum, t.p, t.n); tp_bayt(&tum, '\n'); tp_birak(&t); }
    }
    if (tum.p) icerik_yorumla(&y, tum.p, tum.n);
    tp_birak(&tum);
    for (int i = 0; i < y.yazi_say; i++) if (g->yazilar[i].e) { up_biraktir(g->yazilar[i].e); g->yazilar[i].e = 0; }
    tp_satir(g->cikti);
    tp_ek(g->cikti, "\f\n", 2);
    g->sayfa++;
}

static void dugum_gez(gez_t *g, int64_t num, iz_t kaynak, int derin) {
    if (derin > PDF_DERIN_MAX || ++g->ziyaret > 60000) return;
    nesne_t o; iz_t sz;
    if (!nesne_al(g->d, num, &o) || !govde_sozluk(o, &sz)) return;
    iz_t rv;
    if (sozluk_al(sz, "/Resources", &rv)) kaynak = rv;
    iz_t kids;
    if (sozluk_al(sz, "/Kids", &kids)) {
        iz_t ka = coz(g->d, kids);
        const uint8_t *p = ka.p + 1, *s = ka.p + ka.n;
        while (ka.n && p < s) {
            p = bos_atla(p, s);
            if (p >= s || *p == ']') break;
            iz_t v; v.p = p;
            const uint8_t *q = deger_atla(p, s, 0);
            const uint8_t *q2 = bos_atla(q, s);
            const uint8_t *q3 = deger_atla(q2, s, 0);
            const uint8_t *q4 = bos_atla(q3, s);
            if (q4 < s && *q4 == 'R') {
                v.n = (uint32_t)(q4 + 1 - p);
                int64_t r = basvuru(v);
                if (r >= 0) dugum_gez(g, r, kaynak, derin + 1);
                p = q4 + 1;
            } else break;
        }
        return;
    }
    uint32_t k;
    if (ara_bellek(sz.p, sz.n, "/Contents", &k)) sayfa_isle(g, sz, kaynak);
}

/* ---------------------------------------------------------------- paragraflama */
static void paragrafla(const char *ham, uint32_t hn, tampon_t *o) {
    uint32_t enuz = 0;
    for (uint32_t i = 0, b = 0; i <= hn; i++) {
        if (i == hn || ham[i] == '\n') { uint32_t u = i - b; if (u > enuz && u < 400) enuz = u; b = i + 1; }
    }
    uint32_t i = 0;
    int acik = 0;                      /* geçerli paragrafta yazı var mı */
    char son = 0; uint32_t son_uz = 0;
    while (i <= hn) {
        uint32_t b = i;
        while (i < hn && ham[i] != '\n') i++;
        uint32_t e = i; i++;
        while (b < e && (ham[b] == ' ' || ham[b] == '\t' || ham[b] == '\r')) b++;
        while (e > b && (ham[e - 1] == ' ' || ham[e - 1] == '\t' || ham[e - 1] == '\r')) e--;
        if (e == b || (e == b + 1 && ham[b] == '\f')) {
            if (acik) { tp_ek(o, "\n\n", 2); acik = 0; son = 0; son_uz = 0; }
            continue;
        }
        if (acik) {
            if (son == '-' && ham[b] >= 'a' && ham[b] <= 'z') { o->n--; o->p[o->n] = 0; }
            else if (son_uz * 10 < enuz * 6 && (son == '.' || son == '!' || son == '?' || son == ':'))
                tp_ek(o, "\n\n", 2);
            else tp_bayt(o, ' ');
        }
        tp_ek(o, ham + b, e - b);
        acik = 1; son = ham[e - 1]; son_uz = e - b;
    }
}

int pdf_mi(const uint8_t *b, uint32_t n) {
    if (n < 8) return 0;
    uint32_t lim = n < 1024 ? n : 1024, k;
    return ara_bellek(b, lim, "%PDF-", &k);
}

int pdf_metin(const uint8_t *pdf, uint32_t n, char *cik, uint32_t cn, int *sayfa_sayi) {
    if (sayfa_sayi) *sayfa_sayi = 0;
    if (!pdf_mi(pdf, n) || cn < 2) return PDF_BOZUK;
    uint32_t k;
    if (ara_bellek(pdf, n, "/Encrypt", &k)) return PDF_SIFRELI;
    pdf_t d; um_bellek_doldur(&d, 0, sizeof d);
    d.pdf = pdf; d.pn = n;
    d.tab = (nesne_t *)up_ayir((uint64_t)PDF_NESNE_MAX * sizeof(nesne_t));
    if (!d.tab) return PDF_BOZUK;
    um_bellek_doldur(d.tab, 0, (uint64_t)PDF_NESNE_MAX * sizeof(nesne_t));
    nesneleri_tara(&d);
    nesne_akislari(&d);

    gez_t g; um_bellek_doldur(&g, 0, sizeof g);
    tampon_t ham; ham.p = 0; ham.n = ham.kap = 0; ham.tasti = 0; ham.sinir = PDF_CIKTI_MAX;
    g.d = &d; g.cikti = &ham;
    g.yazilar = (yazi_t *)up_ayir((uint64_t)PDF_YAZI_MAX * sizeof(yazi_t));
    int sonuc = PDF_BOZUK;
    if (g.yazilar) {
        um_bellek_doldur(g.yazilar, 0, (uint64_t)PDF_YAZI_MAX * sizeof(yazi_t));
        /* kök: son "/Root N G R" */
        int64_t kok = -1;
        for (uint32_t i = 0; i + 6 < n; i++)
            if (pdf[i] == '/' && bel_esit(pdf + i, "/Root", 5)) {
                iz_t v; v.p = pdf + i + 5; while (v.p < pdf + n && bosluk(*v.p)) v.p++;
                v.n = (uint32_t)(pdf + n - v.p < 40 ? pdf + n - v.p : 40);
                int64_t r = basvuru(v);
                if (r >= 0) kok = r;
            }
        iz_t bos; bos.p = (const uint8_t *)""; bos.n = 0;
        if (kok >= 0) {
            nesne_t o; iz_t sz, pv;
            if (nesne_al(&d, kok, &o) && govde_sozluk(o, &sz) && sozluk_al(sz, "/Pages", &pv)) {
                int64_t pr = basvuru(pv);
                if (pr >= 0) dugum_gez(&g, pr, bos, 0);
            }
        }
        if (g.sayfa == 0) {           /* ağaç okunamadıysa: tüm /Page nesneleri numara sırasıyla */
            for (uint32_t num = 1; num < PDF_NESNE_MAX; num++) {
                nesne_t o; iz_t sz;
                if (!nesne_al(&d, num, &o) || !govde_sozluk(o, &sz)) continue;
                uint32_t q;
                if (ara_bellek(sz.p, sz.n, "/Type/Page", &q) || ara_bellek(sz.p, sz.n, "/Type /Page", &q)) {
                    if (ara_bellek(sz.p, sz.n, "/Pages", &q)) continue;
                    dugum_gez(&g, num, bos, 0);
                }
            }
        }
        if (g.sayfa > 0) {
            tampon_t par; par.p = 0; par.n = par.kap = 0; par.tasti = 0; par.sinir = PDF_CIKTI_MAX;
            paragrafla((const char *)(ham.p ? ham.p : (const uint8_t *)""), ham.n, &par);
            while (par.n && (par.p[par.n - 1] == '\n' || par.p[par.n - 1] == ' ')) { par.n--; par.p[par.n] = 0; }
            if (par.n < 8) sonuc = PDF_METINSIZ;
            else {
                uint32_t k2 = par.n < cn - 1 ? par.n : cn - 1;
                /* UTF-8 ortasında kesme */
                while (k2 > 0 && k2 < par.n && (par.p[k2] & 0xC0) == 0x80) k2--;
                um_bellek_kopya(cik, par.p, k2); cik[k2] = 0;
                sonuc = (int)k2;
                if (sayfa_sayi) *sayfa_sayi = g.sayfa;
            }
            tp_birak(&par);
        } else sonuc = PDF_BOZUK;
        up_biraktir(g.yazilar);
    }
    tp_birak(&ham);
    for (int i = 0; i < d.tut_say; i++) up_biraktir(d.tut[i]);
    if (d.tut) up_biraktir(d.tut);
    up_biraktir(d.tab);
    return sonuc;
}

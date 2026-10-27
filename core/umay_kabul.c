#include "umay_kabul.h"
#include "umay_bilgi.h"
#include "umay_ozan.h"
#include "umay_ogren.h"
#include "umay_ortak.h"
#include "umay_pdf.h"
#include "umay_ceviri.h"
#include "umay_port.h"
#include "umay_str.h"

#define PARCA_BAYT   2400
#define PARCA_MAX    512
#define DOSYA_MAX    (64 * 1024 * 1024)
#define METIN_MAX    (4 * 1024 * 1024)

static uint32_t fnv(const char *s, uint32_t n) {
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < n; i++) { h ^= (uint8_t)s[i]; h *= 16777619u; }
    return h;
}

static void hex8(char *c, uint32_t h) {
    static const char H[] = "0123456789abcdef";
    for (int i = 7; i >= 0; i--) { c[i] = H[h & 15]; h >>= 4; }
    c[8] = 0;
}

static char *oku_tum(const char *yol, uint32_t *uz) {
    int64_t boy = up_boy(yol);
    if (boy <= 0 || boy > DOSYA_MAX) return 0;
    char *b = (char *)up_ayir((uint64_t)boy + 1);
    if (!b) return 0;
    up_tut_t t = up_ac(yol, UP_OKU);
    if (t < 0) { up_biraktir(b); return 0; }
    int64_t top = 0;
    while (top < boy) {
        int64_t r = up_oku(t, b + top, (uint64_t)(boy - top));
        if (r <= 0) break;
        top += r;
    }
    up_kapat(t);
    b[top] = 0;
    *uz = (uint32_t)top;
    return b;
}

static int pdf_adi(const char *ad) {
    uint32_t u = um_uzun(ad);
    return u > 4 && um_esit(ad + u - 4, ".pdf");
}
static int metin_dosyasi(const char *ad) {
    uint32_t u = um_uzun(ad);
    return (u > 4 && um_esit(ad + u - 4, ".txt")) || (u > 3 && um_esit(ad + u - 3, ".md")) || pdf_adi(ad);
}

static int islendi_mi(const char *liste, uint32_t h) {
    if (!liste) return 0;
    char an[16];
    hex8(an, h);
    return um_icinde(liste, an) != 0;
}

static void islendi_ekle(const char *kok, char **liste, uint32_t h, const char *ad, int parca) {
    char satir[160], hx[16];
    hex8(hx, h);
    um_bicim(satir, sizeof satir, "%s\t%s\t%d\n", hx, ad, parca);
    uint32_t a = *liste ? um_uzun(*liste) : 0, b = um_uzun(satir);
    char *y = (char *)up_ayir((uint64_t)a + b + 1);
    if (!y) return;
    if (a) um_bellek_kopya(y, *liste, a);
    um_bellek_kopya(y + a, satir, b + 1);
    if (*liste) up_biraktir(*liste);
    *liste = y;
    char yol[UMAY_YOL_MAX];
    um_bicim(yol, sizeof yol, "%s/gelen/_islendi.txt", kok);
    up_yaz_yola(yol, y, a + b);
}

/* belgenin ilk anlamlı satırı (başlık) */
static void ilk_satir(const char *m, char *c, uint32_t n) {
    c[0] = 0;
    while (*m == '\n' || *m == '\r' || *m == ' ' || *m == '#') m++;
    uint32_t o = 0;
    while (*m && *m != '\n' && *m != '\r' && o + 1 < n) c[o++] = *m++;
    while (o && c[o - 1] == ' ') o--;
    c[o] = 0;
    /* UTF-8 ortasında kesilmesin */
    while (o && ((uint8_t)c[o - 1] & 0xC0) == 0x80 && o + 1 >= n) o--;
    c[o] = 0;
}

static int en_iyi(const int *skor, int *ikinci) {
    int b = 0, i2 = 0;
    for (int i = 1; i < ALAN_SAYI; i++) if (skor[i] > skor[b]) b = i;
    for (int i = 1; i < ALAN_SAYI; i++) if (i != b && skor[i] > skor[i2]) i2 = i;
    if (ikinci) *ikinci = i2;
    return b;
}

/* Parçayı <kok>/<kategori>/kabul-<hash>[-b].txt olarak künyeli yazar.
 * 1 = yazıldı, 0 = zaten vardı, -1 = hata. */
static int parca_yaz(const char *kok, const char *kat, uint32_t h, const char *ek,
                     const char *baslik, const char *etiket, const char *kaynak,
                     const char *metin, uint32_t uz) {
    char yol[UMAY_YOL_MAX], hx[16];
    hex8(hx, h);
    um_bicim(yol, sizeof yol, "%s/%s/kabul-%s%s.txt", kok, kat, hx, ek);
    if (up_boy(yol) > 0) return 0;
    char dizin[UMAY_YOL_MAX];
    um_bicim(dizin, sizeof dizin, "%s/%s", kok, kat);
    up_dizin_yarat(dizin);
    uint64_t cap = (uint64_t)uz + 512;
    char *b = (char *)up_ayir(cap);
    if (!b) return -1;
    um_bicim(b, (uint32_t)cap, "#kategori: %s\n#baslik: %s\n#etiket: %s\n#kaynak: %s\n#dil: %s\n\n",
             kat, baslik, etiket, kaynak, ogren_dil_kestir(metin));
    uint32_t a = um_uzun(b);
    um_bellek_kopya(b + a, metin, uz);
    b[a + uz] = 0;
    int r = up_yaz_yola(yol, b, (uint64_t)a + uz);
    up_biraktir(b);
    if (r < 0) return -1;
    bilgi_belge_ekle(yol);
    /* yabancı dilde ise Türkçe çeviri için sıraya al (ÇEVİRMEN) */
    if (!um_esit(ogren_dil_kestir(metin), "tr")) ceviri_kayit(yol);
    return 1;
}

int kabul_bekleyen(const char *kok) {
    char gd[UMAY_YOL_MAX];
    um_bicim(gd, sizeof gd, "%s/gelen", kok);
    up_girdi_t g[32];
    int adet = up_dizin_listele(gd, g, 0, 32), n = 0;
    uint32_t lu = 0;
    char lyol[UMAY_YOL_MAX];
    um_bicim(lyol, sizeof lyol, "%s/gelen/_islendi.txt", kok);
    char *liste = oku_tum(lyol, &lu);
    for (int i = 0; i < adet; i++) {
        if (g[i].dizin_mi || !metin_dosyasi(g[i].ad) || g[i].ad[0] == '_') continue;
        char yol[UMAY_YOL_MAX];
        um_bicim(yol, sizeof yol, "%s/%s", gd, g[i].ad);
        uint32_t uz = 0; char *m = oku_tum(yol, &uz);
        if (m) { if (!islendi_mi(liste, fnv(m, uz))) n++; up_biraktir(m); }
    }
    if (liste) up_biraktir(liste);
    return n;
}

int kabul_calistir(const char *kok, kabul_sonuc_t *s, char *rapor, uint32_t n) {
    kabul_sonuc_t bos; if (!s) s = &bos;
    um_bellek_doldur(s, 0, sizeof *s);
    um_kopya(rapor, n, "");
    char gd[UMAY_YOL_MAX];
    um_bicim(gd, sizeof gd, "%s/gelen", kok);
    up_dizin_yarat(gd);
    char lyol[UMAY_YOL_MAX]; uint32_t lu = 0;
    um_bicim(lyol, sizeof lyol, "%s/_islendi.txt", gd);
    char *liste = oku_tum(lyol, &lu);

    up_girdi_t g[32];
    int adet = up_dizin_listele(gd, g, 0, 32);
    if (adet <= 0) {
        um_ekle(rapor, n, "gelen/ boş. Belgelerini (.txt, .md) şuraya at: ");
        um_ekle(rapor, n, gd); um_ekle(rapor, n, "\n");
        if (liste) up_biraktir(liste);
        return 0;
    }

    for (int i = 0; i < adet; i++) {
        if (g[i].dizin_mi || g[i].ad[0] == '_') continue;
        if (!metin_dosyasi(g[i].ad)) {
            s->desteklenmeyen++;
            um_ekle(rapor, n, "  ! "); um_ekle(rapor, n, g[i].ad);
            um_ekle(rapor, n, " metin değil (.txt/.md'ye çevirip yeniden at)\n");
            continue;
        }
        char yol[UMAY_YOL_MAX];
        um_bicim(yol, sizeof yol, "%s/%s", gd, g[i].ad);
        uint32_t boy = 0; char *m = oku_tum(yol, &boy);
        if (!m) { um_ekle(rapor, n, "  ! "); um_ekle(rapor, n, g[i].ad); um_ekle(rapor, n, " okunamadı ya da 2 MB'tan büyük\n"); continue; }
        uint32_t hd = fnv(m, boy);
        if (islendi_mi(liste, hd)) { s->tekrar++; up_biraktir(m); continue; }
        s->dosya++;
        if (pdf_adi(g[i].ad) || pdf_mi((const uint8_t *)m, boy)) {
            char *metin = (char *)up_ayir(METIN_MAX);
            int sayfa = 0, pr = metin ? pdf_metin((const uint8_t *)m, boy, metin, METIN_MAX, &sayfa) : PDF_BOZUK;
            up_biraktir(m);
            if (pr <= 0) {
                const char *ned = pr == PDF_SIFRELI ? "şifreli PDF (şifresiz kopyasını at)"
                                : pr == PDF_METINSIZ ? "metin katmanı yok: taranmış görüntü olabilir, OCR gerekir"
                                : "PDF okunamadı (bozuk ya da desteklenmeyen yapı)";
                um_ekle(rapor, n, "  ! "); um_ekle(rapor, n, g[i].ad); um_ekle(rapor, n, " — "); um_ekle(rapor, n, ned); um_ekle(rapor, n, "\n");
                s->desteklenmeyen++;
                islendi_ekle(kok, &liste, hd, g[i].ad, 0);
                if (metin) up_biraktir(metin);
                continue;
            }
            boy = (uint32_t)pr;
            m = metin;
            { char t[48]; um_bicim(t, sizeof t, "  (PDF: %d sayfa, %d bayt metin)\n", sayfa, pr); um_ekle(rapor, n, t); }
        }

        char baslik0[72]; ilk_satir(m, baslik0, sizeof baslik0);
        if (!baslik0[0]) um_kopya(baslik0, sizeof baslik0, g[i].ad);
        int kat_say[ALAN_SAYI + 1]; for (int q = 0; q <= ALAN_SAYI; q++) kat_say[q] = 0;
        int yaz = 0, bel = 0, kat_parca = 0;

        /* 2. KÜNYELİ: olduğu gibi kendi alanına */
        if (um_bas_mi(m, "#kategori:")) {
            char kat[32]; kat[0] = 0;
            const char *p = m + 10; while (*p == ' ') p++;
            uint32_t o = 0; while (*p && *p != '\n' && *p != '\r' && o + 1 < sizeof kat) kat[o++] = *p++;
            kat[o] = 0;
            int gecerli = 0;
            for (int a = 1; a < ALAN_SAYI; a++) if (um_esit(kat, bilgi_alan_kategorisi((alan_t)a))) gecerli = 1;
            if (gecerli || um_esit(kat, "ortak")) {
                char yol2[UMAY_YOL_MAX], dz[UMAY_YOL_MAX], hx2[16];
                hex8(hx2, hd);
                um_bicim(dz, sizeof dz, "%s/%s", kok, kat);
                up_dizin_yarat(dz);
                um_bicim(yol2, sizeof yol2, "%s/kabul-%s.txt", dz, hx2);
                if (up_boy(yol2) <= 0 && up_yaz_yola(yol2, m, boy) >= 0) {
                    bilgi_belge_ekle(yol2); s->yeni_kayit++; yaz = 1; s->parca++;
                }
                um_ekle(rapor, n, "  ✓ "); um_ekle(rapor, n, g[i].ad);
                um_ekle(rapor, n, " → "); um_ekle(rapor, n, kat); um_ekle(rapor, n, " (künyesine göre)\n");
                islendi_ekle(kok, &liste, hd, g[i].ad, yaz);
                up_biraktir(m);
                continue;
            }
        }

        /* 3. İNCELEME: künyesiz — parçala, puanla */
        /* paragraf aralıkları (boş satırla ayrılmış); uzun olan boyuta göre bölünür */
        uint32_t *pb = (uint32_t *)up_ayir(PARCA_MAX * 4), *pu = (uint32_t *)up_ayir(PARCA_MAX * 4);
        int *pk = (int *)up_ayir(PARCA_MAX * 4);
        if (!pb || !pu || !pk) { if (pb) up_biraktir(pb); if (pu) up_biraktir(pu); if (pk) up_biraktir(pk); up_biraktir(m); break; }
        int npar = 0;
        for (uint32_t q = 0; q < boy && npar < PARCA_MAX; ) {
            while (q < boy && (m[q] == '\n' || m[q] == '\r')) q++;
            if (q >= boy) break;
            uint32_t bs = q;
            while (q < boy) {
                if (m[q] == '\n' && q + 1 < boy && (m[q + 1] == '\n' || (m[q + 1] == '\r' && q + 2 < boy && m[q + 2] == '\n'))) break;
                q++;
            }
            uint32_t ul = q - bs;
            if (ul > PARCA_BAYT) {
                char *tmp = (char *)up_ayir((uint64_t)ul + 1);
                if (tmp) {
                    um_bellek_kopya(tmp, m + bs, ul); tmp[ul] = 0;
                    uint32_t sb[32], su[32];
                    int ns = ogren_parcala(tmp, PARCA_BAYT, sb, su, 32);
                    for (int x = 0; x < ns && npar < PARCA_MAX; x++) { pb[npar] = bs + sb[x]; pu[npar] = su[x]; npar++; }
                    up_biraktir(tmp);
                }
            } else { pb[npar] = bs; pu[npar] = ul; npar++; }
        }
        /* her paragrafın alanı: kendi puanı → (zayıfsa) öncekinin alanı → bilgi tabanı → ortak */
        int onceki = -1;
        for (int x = 0; x < npar; x++) {
            char *pm = (char *)up_ayir((uint64_t)pu[x] + 1);
            pk[x] = -1;
            if (!pm) continue;
            um_bellek_kopya(pm, m + pb[x], pu[x]); pm[pu[x]] = 0;
            int sk[ALAN_SAYI]; ozan_alan_skorla(pm, sk);
            int bb = en_iyi(sk, 0);
            if (bb > 0 && sk[bb] >= 2) pk[x] = bb;
            else if (bb > 0 && sk[bb] == 1 && onceki > 0 && pu[x] >= 100) pk[x] = onceki;
            else {
                baglam_t bg[1]; char kes[320]; uint32_t kz = pu[x] < 300 ? pu[x] : 300;
                um_bellek_kopya(kes, pm, kz); kes[kz] = 0;
                if (kz >= 100 && bilgi_baglam_bul(kes, bg, 1) > 0 && bg[0].skor >= 4) {
                    int al = (int)bilgi_kategori_alani(bg[0].kategori);
                    if (al > 0) pk[x] = al;
                }
            }
            if (pk[x] > 0) onceki = pk[x];
            up_biraktir(pm);
        }
        /* aynı alandaki bitişik paragrafları birleştir (en çok PARCA_BAYT) */
        for (int x = 0; x < npar; ) {
            uint32_t cb = pb[x], ce = pb[x] + pu[x];
            int y = x + 1;
            while (y < npar && pk[y] == pk[x] && (pb[y] + pu[y]) - cb <= PARCA_BAYT) { ce = pb[y] + pu[y]; y++; }
            uint32_t cu = ce - cb;
            char *pm = (char *)up_ayir((uint64_t)cu + 1);
            if (pm) {
                um_bellek_kopya(pm, m + cb, cu); pm[cu] = 0;
                uint32_t hp = fnv(pm, cu);
                int b = pk[x] > 0 ? pk[x] : 0;
                const char *kat = b ? bilgi_alan_kategorisi((alan_t)b) : "ortak";
                if (!b) { bel++; s->belirsiz++; }
                char bas_t[96], et[96];
                um_bicim(bas_t, sizeof bas_t, "%s (bölüm %d)", baslik0, ++kat_parca);
                um_bicim(et, sizeof et, "kabul,%s", kat);
                int r = parca_yaz(kok, kat, hp, "", bas_t, et, g[i].ad, pm, cu);
                if (r > 0) { yaz++; s->parca++; s->yeni_kayit++; kat_say[b]++; } else if (r == 0) s->tekrar++;
                /* çok alanlı: ikinci alan yakın puandaysa ona da */
                if (b) {
                    int sk[ALAN_SAYI]; ozan_alan_skorla(pm, sk);
                    int i2; int b2 = en_iyi(sk, &i2);
                    if (i2 > 0 && i2 != b && sk[i2] >= 3 && sk[i2] * 3 >= sk[b2] * 2 && sk[b] >= 2) {
                        const char *k2 = bilgi_alan_kategorisi((alan_t)i2);
                        char et2[96]; um_bicim(et2, sizeof et2, "kabul,%s,%s", k2, kat);
                        if (parca_yaz(kok, k2, hp, "-b", bas_t, et2, g[i].ad, pm, cu) > 0) { s->parca++; s->yeni_kayit++; kat_say[i2]++; }
                    }
                }
                up_biraktir(pm);
            }
            x = y;
        }
        up_biraktir(pb); up_biraktir(pu); up_biraktir(pk);
        /* satır raporu */
        um_ekle(rapor, n, "  ✓ "); um_ekle(rapor, n, g[i].ad); um_ekle(rapor, n, " →");
        for (int a = 0; a < ALAN_SAYI; a++) if (kat_say[a]) {
            char t[64];
            um_bicim(t, sizeof t, " %s %d,", a ? bilgi_alan_kategorisi((alan_t)a) : "ortak", kat_say[a]);
            um_ekle(rapor, n, t);
        }
        if (bel) { char t[48]; um_bicim(t, sizeof t, " (belirsiz %d parça: gözden geçir)", bel); um_ekle(rapor, n, t); }
        um_ekle(rapor, n, "\n");
        islendi_ekle(kok, &liste, hd, g[i].ad, yaz);
        up_biraktir(m);
    }
    if (liste) up_biraktir(liste);
    if (ceviri_bekleyen() > 0) {
        int cv = ogren_acik() ? ceviri_gecis(10) : 0;
        char t[160];
        um_bicim(t, sizeof t, "ÇEVİRMEN: %d yabancı dilli parça Türkçeye çevrildi, %d sırada (ağ açılınca çevrilir)\n", cv, ceviri_bekleyen());
        um_ekle(rapor, n, t);
    }
    char son[160];
    um_bicim(son, sizeof son, "KABUL: %d belge, %d parça yerleşti, %d belirsiz, %d atlandı%s\n",
             s->dosya, s->parca, s->belirsiz, s->tekrar,
             s->desteklenmeyen ? ", desteklenmeyen dosya var" : "");
    um_ekle(rapor, n, son);
    return s->parca;
}

void kabul_bosta(void) {
    static uint64_t son_ms;
    uint64_t simdi = up_ms();
    if (son_ms && simdi - son_ms < 30000) return;
    son_ms = simdi;
    const char *kok = ayar_metin("kutuphane", "kut");
    if (kabul_bekleyen(kok) <= 0) return;
    kabul_sonuc_t s; static char r[2048];
    kabul_calistir(kok, &s, r, sizeof r);
    gun_yaz("%s", r);
}

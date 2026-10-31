/* KASA — bkz. umay_kasa.h. Şifreleme Monocypher'dan; burada yalnız biçim, durum ve kayıt yönetimi var. */
#include "umay_kasa.h"
#include "umay_port.h"
#include "umay_str.h"
#include "monocypher.h"

#define BASLIK_N   77u      /* magic 8 + sürüm 1 + bloklar 4 + geçiş 4 + tuz 16 + nonce 24 + boy 4 + mac 16 */
#define AD_N       61u      /* kimlik doğrulanan kısım: ilk 61 bayt (mac'e kadar) */
#define SAYFA      4096u

static char     g_yol[320];
static int      g_yol_ozel;
static uint32_t g_bloklar = 65536, g_gecis = 3;
static uint32_t g_kilit_ms = 300000;

static int      g_acik;
static uint8_t  g_anahtar[32], g_tuz[16];
static uint32_t g_ab, g_ag;
static uint8_t *g_veri;          /* düz metin kayıtları: [u32 adet] + { u8 tur, u8 adlen, ad, u32 boy, veri } */
static uint32_t g_veri_n;
static uint64_t g_son;

void kasa_yol_ata(const char *yol) { if (yol && *yol) { um_kopya(g_yol, sizeof g_yol, yol); g_yol_ozel = 1; } else g_yol_ozel = 0; }
void kasa_parametre_ata(uint32_t b, uint32_t g) { if (b >= 8 && g >= 1) { g_bloklar = b; g_gecis = g; } }
void kasa_kilit_suresi_ata(uint32_t ms) { g_kilit_ms = ms; }

static const char *yol_al(void) { if (!g_yol_ozel) up_kut_dosya("kasa.bin", g_yol, sizeof g_yol); return g_yol; }
static void yedek_yolu(char *c, uint32_t n) {
    if (g_yol_ozel) { um_kopya(c, n, g_yol); um_ekle(c, n, ".yed"); } else up_kut_dosya("kasa.yed", c, n);
}

static void y32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static uint32_t o32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

int kasa_var_mi(void) { return up_boy(yol_al()) > 0; }

/* ---------- durum ---------- */
void kasa_kilitle(void) {
    if (g_veri) { crypto_wipe(g_veri, KASA_AZAMI + 8); up_biraktir(g_veri); g_veri = 0; }
    crypto_wipe(g_anahtar, sizeof g_anahtar);
    g_veri_n = 0; g_acik = 0;
}
int kasa_acik_mi(void) {
    if (!g_acik) return 0;
    if (up_ms() - g_son > g_kilit_ms) { kasa_kilitle(); return 0; }
    return 1;
}
static void dokundu(void) { g_son = up_ms(); }

/* ---------- anahtar ---------- */
static int turet(const char *parola, const uint8_t *tuz, uint32_t bloklar, uint32_t gecis, uint8_t cik[32]) {
    void *is = up_ayir((uint64_t)bloklar * 1024);
    if (!is) return -4;
    crypto_argon2_config c = { CRYPTO_ARGON2_ID, bloklar, gecis, 1 };
    crypto_argon2_inputs g = { (const uint8_t *)parola, tuz, um_uzun(parola), 16 };
    crypto_argon2(cik, 32, is, c, g, crypto_argon2_no_extras);
    crypto_wipe(is, (size_t)bloklar * 1024);
    up_biraktir(is);
    return 0;
}

/* ---------- diske yaz ---------- */
static int kaydet(void) {
    if (!g_acik || !g_veri) return -6;
    uint32_t dolgulu = (g_veri_n + SAYFA - 1) / SAYFA * SAYFA;       /* boyutu gizlemek için sayfaya yuvarla */
    if (dolgulu == 0) dolgulu = SAYFA;
    uint8_t *dosya = (uint8_t *)up_ayir((uint64_t)BASLIK_N + dolgulu);
    uint8_t *duz = (uint8_t *)up_ayir(dolgulu);
    if (!dosya || !duz) { if (dosya) up_biraktir(dosya); if (duz) up_biraktir(duz); return -4; }
    um_bellek_doldur(duz, 0, dolgulu);
    um_bellek_kopya(duz, g_veri, g_veri_n);
    um_bellek_kopya(dosya, "UMAYKASA", 8);
    dosya[8] = 1;
    y32(dosya + 9, g_ab); y32(dosya + 13, g_ag);
    um_bellek_kopya(dosya + 17, g_tuz, 16);
    if (up_rastgele(dosya + 33, 24) != 0) { crypto_wipe(duz, dolgulu); up_biraktir(dosya); up_biraktir(duz); return -3; }
    y32(dosya + 57, dolgulu);
    crypto_aead_lock(dosya + BASLIK_N, dosya + 61, g_anahtar, dosya + 33, dosya, AD_N, duz, dolgulu);
    /* mac AD'ye girmez (61..76), şifreli metin 77'den başlar */
    crypto_wipe(duz, dolgulu);
    up_biraktir(duz);

    /* öncekini yedekle (yazım yarıda kesilirse kayıp olmasın) */
    {
        int64_t eski = up_boy(yol_al());
        if (eski > 0 && eski < (int64_t)KASA_AZAMI + 2 * SAYFA) {
            uint8_t *e = (uint8_t *)up_ayir((uint64_t)eski);
            if (e) {
                up_tut_t t = up_ac(yol_al(), UP_OKU);
                int64_t top = 0;
                if (t != UP_GECERSIZ) { while (top < eski) { int64_t k = up_oku(t, e + top, (uint64_t)(eski - top)); if (k <= 0) break; top += k; } up_kapat(t); }
                if (top == eski) { char y[340]; yedek_yolu(y, sizeof y); up_yaz_yola(y, e, (uint64_t)eski); up_ozel_kutuk(y); }
                up_biraktir(e);
            }
        }
    }
    int r = up_yaz_yola(yol_al(), dosya, (uint64_t)BASLIK_N + dolgulu);
    if (r >= 0) up_ozel_kutuk(yol_al());
    up_biraktir(dosya);
    return r >= 0 ? 0 : -4;
}

/* ---------- kayıt gezinti ---------- */
/* off: kayıt başı. Dönen: 1 = geçerli kayıt, değerler doldurulur; 0 = sonu/bozuk. */
static int kayit_oku(uint32_t off, uint32_t *tur, const uint8_t **ad, uint32_t *adn, const uint8_t **veri, uint32_t *boy, uint32_t *sonraki) {
    if (off + 2 > g_veri_n) return 0;
    *tur = g_veri[off]; *adn = g_veri[off + 1];
    if (*adn == 0 || *adn > KASA_AD_MAX || off + 2 + *adn + 4 > g_veri_n) return 0;
    *ad = g_veri + off + 2;
    uint32_t b = o32(g_veri + off + 2 + *adn);
    uint32_t v = off + 2 + *adn + 4;
    if (b > g_veri_n || v + b > g_veri_n) return 0;
    *veri = g_veri + v; *boy = b; *sonraki = v + b;
    return 1;
}
static int dogru_yapi(void) {
    if (g_veri_n < 4) return 0;
    uint32_t say = o32(g_veri), off = 4;
    for (uint32_t i = 0; i < say; i++) {
        uint32_t t, an, b, so; const uint8_t *a, *v;
        if (!kayit_oku(off, &t, &a, &an, &v, &b, &so)) return 0;
        off = so;
    }
    g_veri_n = off;               /* dolgu atılır */
    return 1;
}
static int ad_bul(const char *ad, uint32_t *bas, uint32_t *son) {
    uint32_t say = o32(g_veri), off = 4, adn = um_uzun(ad);
    for (uint32_t i = 0; i < say; i++) {
        uint32_t t, an, b, so; const uint8_t *a, *v;
        if (!kayit_oku(off, &t, &a, &an, &v, &b, &so)) return 0;
        if (an == adn && um_esit_n((const char *)a, ad, adn)) { *bas = off; *son = so; return 1; }
        off = so;
    }
    return 0;
}

/* ---------- aç / kur ---------- */
static int durum_hazirla(void) {
    kasa_kilitle();
    g_veri = (uint8_t *)up_ayir(KASA_AZAMI + 8);
    if (!g_veri) return -4;
    um_bellek_doldur(g_veri, 0, KASA_AZAMI + 8);
    return 0;
}

int kasa_kur(const char *parola) {
    if (!parola || um_uzun(parola) < KASA_PAROLA_AZ) return -5;
    if (kasa_var_mi()) return -1;
    if (up_rastgele(g_tuz, 16) != 0) return -3;
    int r = durum_hazirla();
    if (r) return r;
    g_ab = g_bloklar; g_ag = g_gecis;
    r = turet(parola, g_tuz, g_ab, g_ag, g_anahtar);
    if (r) { kasa_kilitle(); return r; }
    y32(g_veri, 0); g_veri_n = 4; g_acik = 1; dokundu();
    r = kaydet();
    if (r) { kasa_kilitle(); return r; }
    return 0;
}

static int ac_ic(const char *parola, const uint8_t *hazir_anahtar) {
    if (!hazir_anahtar && (!parola || !*parola)) return -2;
    int64_t boy = up_boy(yol_al());
    if (boy <= 0) return -1;
    if (boy < (int64_t)BASLIK_N + SAYFA || boy > (int64_t)BASLIK_N + KASA_AZAMI + SAYFA) return -2;
    uint8_t *d = (uint8_t *)up_ayir((uint64_t)boy);
    if (!d) return -4;
    up_tut_t t = up_ac(yol_al(), UP_OKU);
    int64_t top = 0;
    if (t != UP_GECERSIZ) { while (top < boy) { int64_t k = up_oku(t, d + top, (uint64_t)(boy - top)); if (k <= 0) break; top += k; } up_kapat(t); }
    int sonuc = -2;
    uint32_t bl = 0, ge = 0, cn = 0;
    if (top != boy || !um_esit_n((const char *)d, "UMAYKASA", 8) || d[8] != 1) goto bitti;
    bl = o32(d + 9); ge = o32(d + 13); cn = o32(d + 57);
    if (bl < 8 || bl > 262144 || ge < 1 || ge > 12) goto bitti;           /* parametre sınırı: hizmet engelleme yok */
    if (cn == 0 || cn % SAYFA != 0 || (int64_t)cn + BASLIK_N != boy || cn > KASA_AZAMI + SAYFA) goto bitti;
    {
        int r = durum_hazirla();
        if (r) { sonuc = r; goto bitti; }
        uint8_t anahtar[32];
        if (hazir_anahtar) um_bellek_kopya(anahtar, hazir_anahtar, 32);
        else { r = turet(parola, d + 17, bl, ge, anahtar); if (r) { sonuc = r; kasa_kilitle(); goto bitti; } }
        int kot = crypto_aead_unlock(g_veri, d + 61, anahtar, d + 33, d, AD_N, d + BASLIK_N, cn);
        if (kot != 0) { crypto_wipe(anahtar, 32); kasa_kilitle(); sonuc = -2; goto bitti; }
        um_bellek_kopya(g_anahtar, anahtar, 32); crypto_wipe(anahtar, 32);
        um_bellek_kopya(g_tuz, d + 17, 16);
        g_ab = bl; g_ag = ge; g_veri_n = cn;
        if (!dogru_yapi()) { kasa_kilitle(); sonuc = -2; goto bitti; }
        g_acik = 1; dokundu(); sonuc = 0;
    }
bitti:
    crypto_wipe(d, (size_t)boy);
    up_biraktir(d);
    return sonuc;
}

int kasa_ac(const char *parola) { return ac_ic(parola, 0); }

/* BİYOMETRİK KISAYOL: uygulama, kasa açıkken anahtarı alıp İŞLETİM SİSTEMİNİN biyometrik korumalı deposuna
 * (iOS Keychain+Face ID, Android Keystore+BiometricPrompt, Windows Hello) koyar; sonra yüz/parmak izi
 * onayından geçen anahtarı kasa_ac_anahtarla ile verir. Anahtar ana parola değişince geçersiz olur. */
int kasa_anahtari_ver(uint8_t cik[32]) {
    if (!kasa_acik_mi()) return -6;
    um_bellek_kopya(cik, g_anahtar, 32);
    return 0;
}
int kasa_ac_anahtarla(const uint8_t anahtar[32]) { return anahtar ? ac_ic(0, anahtar) : -2; }

/* ---------- işlemler ---------- */
int kasa_say(void) { if (!kasa_acik_mi()) return -6; dokundu(); return (int)o32(g_veri); }

int kasa_bilgi(int i, char *ad, uint32_t n, int *tur, uint32_t *boy) {
    if (!kasa_acik_mi()) return -6;
    dokundu();
    uint32_t say = o32(g_veri), off = 4;
    for (uint32_t k = 0; k < say; k++) {
        uint32_t t, an, b, so; const uint8_t *a, *v;
        if (!kayit_oku(off, &t, &a, &an, &v, &b, &so)) return -1;
        if ((int)k == i) {
            if (ad && n) { uint32_t c = an < n - 1 ? an : n - 1; um_bellek_kopya(ad, a, c); ad[c] = 0; }
            if (tur) *tur = (int)t;
            if (boy) *boy = b;
            return 0;
        }
        off = so;
    }
    return -1;
}

int kasa_yaz(const char *ad, int tur, const void *veri, uint32_t n) {
    if (!kasa_acik_mi()) return -6;
    if (!ad || !*ad || um_uzun(ad) > KASA_AD_MAX) return -1;
    dokundu();
    uint32_t bas, son, adn = um_uzun(ad);
    if (ad_bul(ad, &bas, &son)) {                       /* üstüne yaz: eskisini çıkar */
        um_bellek_kopya(g_veri + bas, g_veri + son, g_veri_n - son);
        g_veri_n -= (son - bas);
        y32(g_veri, o32(g_veri) - 1);
    }
    uint32_t gerek = 2 + adn + 4 + n;
    if (gerek > KASA_AZAMI || g_veri_n + gerek > KASA_AZAMI) return -4;
    g_veri[g_veri_n] = (uint8_t)(tur == KASA_DOSYA ? KASA_DOSYA : KASA_METIN);
    g_veri[g_veri_n + 1] = (uint8_t)adn;
    um_bellek_kopya(g_veri + g_veri_n + 2, ad, adn);
    y32(g_veri + g_veri_n + 2 + adn, n);
    if (n) um_bellek_kopya(g_veri + g_veri_n + 2 + adn + 4, veri, n);
    g_veri_n += gerek;
    y32(g_veri, o32(g_veri) + 1);
    return kaydet();
}

int32_t kasa_boy(const char *ad) {
    if (!kasa_acik_mi()) return -6;
    dokundu();
    uint32_t bas, son;
    if (!ad_bul(ad, &bas, &son)) return -1;
    uint32_t t, an, b, so; const uint8_t *a, *v;
    if (!kayit_oku(bas, &t, &a, &an, &v, &b, &so)) return -1;
    return (int32_t)b;
}

int32_t kasa_oku(const char *ad, void *cik, uint32_t n) {
    if (!kasa_acik_mi()) return -6;
    dokundu();
    uint32_t bas, son;
    if (!ad_bul(ad, &bas, &son)) return -1;
    uint32_t t, an, b, so; const uint8_t *a, *v;
    if (!kayit_oku(bas, &t, &a, &an, &v, &b, &so)) return -1;
    if (n < b) return -4;
    if (b) um_bellek_kopya(cik, v, b);
    return (int32_t)b;
}

int kasa_sil(const char *ad) {
    if (!kasa_acik_mi()) return -6;
    dokundu();
    uint32_t bas, son;
    if (!ad_bul(ad, &bas, &son)) return -1;
    um_bellek_kopya(g_veri + bas, g_veri + son, g_veri_n - son);
    crypto_wipe(g_veri + g_veri_n - (son - bas), son - bas);
    g_veri_n -= (son - bas);
    y32(g_veri, o32(g_veri) - 1);
    return kaydet();
}

int kasa_parola_degistir(const char *eski, const char *yeni) {
    if (!yeni || um_uzun(yeni) < KASA_PAROLA_AZ) return -5;
    if (kasa_acik_mi()) kasa_kilitle();
    int r = kasa_ac(eski);                               /* eski parola yeniden doğrulanır */
    if (r) return r;
    if (up_rastgele(g_tuz, 16) != 0) { kasa_kilitle(); return -3; }
    r = turet(yeni, g_tuz, g_ab, g_ag, g_anahtar);
    if (r) { kasa_kilitle(); return r; }
    r = kaydet();
    return r;
}

#include "umay_egit.h"
#include "umay_arastir.h"
#include "umay_bilgi.h"
#include "umay_ogren.h"
#include "umay_ortak.h"
#include "umay_port.h"
#include "umay_str.h"

#define KARNE_DOSYA_MAX 14000u
#define EGIT_ARALIK_MS  300000u      /* boşta tur aralığı: 5 dk */
#define KAZANC_BINDE    20           /* %2 */
#define KORUMA_YUZDE    85

static void karne_yolu(char *c, uint32_t n) {
    um_bicim(c, n, "%s/_ogren/karne.txt", ayar_metin("kutuphane", "kut"));
}

/* satır içi tab ve satırsonu temizle */
static void temiz(char *d, uint32_t n, const char *k) {
    uint32_t o = 0;
    for (; k && *k && o + 1 < n; k++) d[o++] = (*k == '\t' || *k == '\n' || *k == '\r') ? ' ' : *k;
    d[o] = 0;
}

void karne_yaz(const char *ajan, const char *soru, int guven, int dayanak) {
    if (!ogren_acik() || !soru || !*soru) return;
    char yol[384];
    karne_yolu(yol, sizeof yol);
    char dizin[384];
    um_bicim(dizin, sizeof dizin, "%s/_ogren", ayar_metin("kutuphane", "kut"));
    up_dizin_yarat(dizin);

    char *t = (char *)up_ayir(KARNE_DOSYA_MAX + 700);
    if (!t) return;
    t[0] = 0;
    up_tut_t h = up_ac(yol, UP_OKU);
    int64_t k = 0;
    if (h >= 0) { k = up_oku(h, t, KARNE_DOSYA_MAX); up_kapat(h); if (k < 0) k = 0; }
    t[k] = 0;
    if ((uint32_t)k + 600 >= KARNE_DOSYA_MAX) {            /* dolunca eski yarıyı at */
        uint32_t at = (uint32_t)k / 2;
        while (at < (uint32_t)k && t[at] != '\n') at++;
        if (at < (uint32_t)k) at++;
        um_bellek_kopya(t, t + at, (uint64_t)k - at + 1);
        k -= at;
    }
    char a[40], q[300];
    temiz(a, sizeof a, ajan);
    temiz(q, sizeof q, soru);
    um_bicim(t + k, 600, "%l\t%s\t%d\t%d\t%s\n", (int64_t)(up_ms() / 1000), a, guven, dayanak, q);
    up_yaz_yola(yol, t, um_uzun(t));
    up_biraktir(t);
}

typedef struct { char ajan[40]; int guven, dayanak; char soru[300]; } karne_satir_t;

/* karneyi okur; satir dizisi heap'te, çağıran bırakır. Dönüş: satır sayısı */
static int karne_oku(karne_satir_t **cikan) {
    *cikan = 0;
    char yol[384];
    karne_yolu(yol, sizeof yol);
    char *t = (char *)up_ayir(KARNE_DOSYA_MAX + 1);
    karne_satir_t *s = (karne_satir_t *)up_ayir(sizeof(karne_satir_t) * KARNE_SORU_MAX);
    if (!t || !s) { if (t) up_biraktir(t); if (s) up_biraktir(s); return 0; }
    up_tut_t h = up_ac(yol, UP_OKU);
    int64_t k = 0;
    if (h >= 0) { k = up_oku(h, t, KARNE_DOSYA_MAX); up_kapat(h); if (k < 0) k = 0; }
    t[k] = 0;
    int n = 0;
    for (char *p = t; *p && n < KARNE_SORU_MAX;) {
        char *satir = p;
        while (*p && *p != '\n') p++;
        if (*p) *p++ = 0;
        char *alan[5]; int a = 0;
        alan[a++] = satir;
        for (char *q = satir; *q && a < 5; q++) if (*q == '\t') { *q = 0; alan[a++] = q + 1; }
        if (a < 5) continue;
        um_kopya(s[n].ajan, sizeof s[n].ajan, alan[1]);
        s[n].guven = alan[2][0] - '0';
        s[n].dayanak = alan[3][0] - '0';
        um_kopya(s[n].soru, sizeof s[n].soru, alan[4]);
        n++;
    }
    up_biraktir(t);
    *cikan = s;
    return n;
}

uint32_t karne_ozet(char *c, uint32_t n) {
    karne_satir_t *s;
    int k = karne_oku(&s);
    c[0] = 0;
    if (k == 0) { if (s) up_biraktir(s); um_kopya(c, n, "Karne boş (henüz kayıtlı yanıt yok ya da öğrenme kapalı).\n"); return um_uzun(c); }
    char adlar[12][40]; int say[12] = {0}, toplam[12] = {0}, yok[12] = {0}, m = 0;
    for (int i = 0; i < k; i++) {
        int j = -1;
        for (int q = 0; q < m; q++) if (um_esit(adlar[q], s[i].ajan)) { j = q; break; }
        if (j < 0 && m < 12) { j = m++; um_kopya(adlar[j], sizeof adlar[j], s[i].ajan); }
        if (j < 0) continue;
        say[j]++; toplam[j] += s[i].guven; if (s[i].dayanak == 0) yok[j]++;
    }
    um_kopya(c, n, "Ajan karnesi (güven 0..3):\n");
    for (int j = 0; j < m; j++) {
        char l[160];
        um_bicim(l, sizeof l, "  %s: %d yanıt, ortalama güven %%%d, dayanaksız %d\n",
                 adlar[j], say[j], (toplam[j] * 100) / (say[j] * 3), yok[j]);
        um_ekle(c, n, l);
    }
    up_biraktir(s);
    return um_uzun(c);
}

/* değerlendirme kümesi: karnedeki benzersiz son sorular */
static int kume_kur(char (*soru)[300], int en_cok, int *zayif) {
    karne_satir_t *s;
    int k = karne_oku(&s), n = 0;
    for (int i = k - 1; i >= 0 && n < en_cok; i--) {
        int var = 0;
        for (int q = 0; q < n; q++) if (um_esit(soru[q], s[i].soru)) { var = 1; break; }
        if (var) continue;
        um_kopya(soru[n], 300, s[i].soru);
        zayif[n] = (s[i].guven <= 1 || s[i].dayanak == 0);
        n++;
    }
    if (s) up_biraktir(s);
    return n;
}

#define KUME_MAX 40

int egit_calistir(int en_cok_bosluk, egit_sonuc_t *sn) {
    um_bellek_doldur(sn, 0, sizeof *sn);
    if (!ogren_acik()) { um_kopya(sn->neden, sizeof sn->neden, "öğrenme kapalı ya da web'e çıkış yolu yok"); return UM_YOK; }

    char (*soru)[300] = (char (*)[300])up_ayir(300 * KUME_MAX);
    int *once = (int *)up_ayir(sizeof(int) * KUME_MAX);
    int zayif[KUME_MAX];
    if (!soru || !once) { if (soru) up_biraktir(soru); if (once) up_biraktir(once); return UM_DOLU; }

    int n = kume_kur(soru, KUME_MAX, zayif);
    sn->soru_sayi = n;
    /* zayıf sorular boşluk olarak kaydedilir (zaten kayıtlıysa dokunulmaz) */
    for (int i = 0; i < n; i++) if (zayif[i]) ogren_bosluk_kaydet("ortak", soru[i]);

    int toplam_once = 0;
    for (int i = 0; i < n; i++) {
        once[i] = arastir_kapsam(soru[i]);
        toplam_once += once[i] * 10;
        if (once[i] >= 50) sn->korunan_once++;
    }
    sn->once = n ? toplam_once / n : 0;

    int n0 = bilgi_sayi();
    ogren_yazilan_sifirla();
    int eklenen = ogren_gecis(en_cok_bosluk);
    if (eklenen == UM_YOK) {
        um_kopya(sn->neden, sizeof sn->neden, "ağ yok; boşluklar bekliyor");
        sn->bekleyen = ogren_bekleyen();
        up_biraktir(soru); up_biraktir(once);
        return UM_YOK;
    }
    sn->eklenen = eklenen > 0 ? eklenen : 0;

    int toplam_sonra = 0;
    for (int i = 0; i < n; i++) {
        int k = arastir_kapsam(soru[i]);
        toplam_sonra += k * 10;
        if (once[i] >= 50 && k >= 50) sn->korunan_sonra++;
    }
    sn->sonra = n ? toplam_sonra / n : 0;
    sn->bekleyen = ogren_bekleyen();

    if (sn->eklenen == 0) {
        um_kopya(sn->neden, sizeof sn->neden, "öğrenilecek yeni bir şey çıkmadı; kütüphane aynı");
    } else if (sn->sonra - sn->once < KAZANC_BINDE) {
        sn->geri_alindi = 1;
        um_bicim(sn->neden, sizeof sn->neden, "kazanç yetersiz (%d → %d binde, en az +%d aranır)", sn->once, sn->sonra, KAZANC_BINDE);
    } else if (sn->korunan_once > 0 && sn->korunan_sonra * 100 < sn->korunan_once * KORUMA_YUZDE) {
        sn->geri_alindi = 1;
        um_bicim(sn->neden, sizeof sn->neden, "eski bilgi bozuldu (%d soruluk %d'i kaldı, en az %%%d korunmalı)", sn->korunan_once, sn->korunan_sonra, KORUMA_YUZDE);
    } else {
        sn->kabul = 1;
        um_bicim(sn->neden, sizeof sn->neden, "kabul: %d → %d binde, %d/%d eski soru korundu", sn->once, sn->sonra, sn->korunan_sonra, sn->korunan_once);
    }

    if (sn->geri_alindi) {
        bilgi_kirp(n0);
        char yol[384];
        for (int i = 0; ogren_yazilan(i, yol, sizeof yol) == UM_OLDU; i++) up_sil(yol);
        gun_yaz("EĞİT: tur geri alındı — %s", sn->neden);
    } else {
        gun_yaz("EĞİT: %s", sn->neden);
    }
    up_biraktir(soru); up_biraktir(once);
    return UM_OLDU;
}

uint32_t egit_rapor(const egit_sonuc_t *s, char *c, uint32_t n) {
    char l[256];
    um_bicim(l, sizeof l, "Eğitim turu: %d soru ölçüldü, %d parça eklendi.\n", s->soru_sayi, s->eklenen);
    um_kopya(c, n, l);
    um_bicim(l, sizeof l, "Kapsam: %d → %d (binde). Eski bilgi korunumu: %d/%d.\n", s->once, s->sonra, s->korunan_sonra, s->korunan_once);
    um_ekle(c, n, l);
    um_bicim(l, sizeof l, "Sonuç: %s — %s\n", s->kabul ? "KABUL" : (s->geri_alindi ? "GERİ ALINDI" : "değişiklik yok"), s->neden);
    um_ekle(c, n, l);
    um_bicim(l, sizeof l, "Bekleyen boşluk: %d\n", s->bekleyen);
    um_ekle(c, n, l);
    return um_uzun(c);
}

static uint64_t g_egit_ms;
static int g_egit_basladi;
void egit_bosta(void) {
    if (!ogren_acik()) return;
    uint64_t s = up_ms();
    if (!g_egit_basladi) { g_egit_basladi = 1; g_egit_ms = s; return; }
    if (s - g_egit_ms < EGIT_ARALIK_MS) return;
    g_egit_ms = s;
    egit_sonuc_t sn;
    egit_calistir(2, &sn);
}

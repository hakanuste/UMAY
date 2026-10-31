/* ANIM — bkz. umay_anim.h. Yalnız um_* ve up_* kullanır (libc yok): ÖTÜKEN'de de derlenir. */
#include "umay_anim.h"
#include "umay_zaman.h"
#include "umay_port.h"
#include "umay_str.h"

static anim_t   g_k[ANIM_MAX];
static char     g_yol[320];
static int      g_yol_ozel;
static int64_t (*g_saat)(void);

void anim_saat_ata(int64_t (*f)(void)) { g_saat = f; }
int64_t anim_simdi(void) { return g_saat ? g_saat() : up_yerel_dakika(); }
void anim_yol_ata(const char *yol) {
    if (yol && *yol) { um_kopya(g_yol, sizeof g_yol, yol); g_yol_ozel = 1; } else g_yol_ozel = 0;
}
static const char *yol_al(void) {
    if (!g_yol_ozel) up_kut_dosya("animsa.txt", g_yol, sizeof g_yol);
    return g_yol;
}

/* ---------- takvim aritmetiği (Howard Hinnant, saf tamsayı) ---------- */
static int64_t gun_say(int y, int m, int d) {
    y -= m <= 2;
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    int64_t yoe = y - era * 400;
    int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}
static void gun_ayir(int64_t z, int *y, int *m, int *d) {
    z += 719468;
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    int64_t doe = z - era * 146097;
    int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int64_t yy = yoe + era * 400;
    int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    int64_t mp = (5 * doy + 2) / 153;
    *d = (int)(doy - (153 * mp + 2) / 5 + 1);
    *m = (int)(mp < 10 ? mp + 3 : mp - 9);
    *y = (int)(yy + (*m <= 2));
}
int64_t anim_dakika(int y, int m, int d, int sa, int dk) { return gun_say(y, m, d) * 1440 + sa * 60 + dk; }
static int64_t gun_bol(int64_t dk) { return dk >= 0 ? dk / 1440 : -((-dk + 1439) / 1440); }
void anim_ayir(int64_t dk, int *y, int *m, int *d, int *sa, int *mi) {
    int64_t g = gun_bol(dk), kalan = dk - g * 1440;
    gun_ayir(g, y, m, d);
    *sa = (int)(kalan / 60); *mi = (int)(kalan % 60);
}
int anim_haftagunu(int64_t dk) {
    int64_t g = gun_bol(dk);
    int64_t w = (g + 3) % 7; if (w < 0) w += 7;      /* 1970-01-01 perşembe */
    return (int)w + 1;
}

static const char *AYLAR[] = { "Ocak", "Şubat", "Mart", "Nisan", "Mayıs", "Haziran", "Temmuz", "Ağustos", "Eylül", "Ekim", "Kasım", "Aralık" };
static const char *GUNAD[] = { "Pazartesi", "Salı", "Çarşamba", "Perşembe", "Cuma", "Cumartesi", "Pazar" };

static void an_yaz(int64_t dk, char *cik, uint32_t n) {
    int y, m, d, sa, mi;
    anim_ayir(dk, &y, &m, &d, &sa, &mi);
    um_bicim(cik, n, "%d %s %s %d.%c%d", d, AYLAR[m - 1], GUNAD[anim_haftagunu(dk) - 1], sa, (char)('0' + mi / 10), mi % 10);
}
static void saat_yaz(int64_t dk, char *cik, uint32_t n) {
    int y, m, d, sa, mi;
    anim_ayir(dk, &y, &m, &d, &sa, &mi);
    um_bicim(cik, n, "%d.%c%d", sa, (char)('0' + mi / 10), mi % 10);
}

/* ---------- kütük ---------- */
static int64_t say_oku(const char **p) {
    int64_t v = 0; int eksi = 0;
    if (**p == '-') { eksi = 1; (*p)++; }
    while (**p >= '0' && **p <= '9') { v = v * 10 + (**p - '0'); (*p)++; }
    return eksi ? -v : v;
}

static void yukle(void) {
    um_bellek_doldur(g_k, 0, sizeof g_k);
    up_tut_t t = up_ac(yol_al(), UP_OKU);
    if (t == UP_GECERSIZ) return;
    static char tam[ANIM_MAX * (ANIM_METIN + 80) + 16];
    uint32_t n = 0;
    for (;;) {
        int64_t k = up_oku(t, tam + n, sizeof tam - 1 - n);
        if (k <= 0) break;
        n += (uint32_t)k;
        if (n + 1 >= sizeof tam) break;
    }
    up_kapat(t);
    tam[n] = 0;
    int s = 0;
    const char *p = tam;
    while (*p && s < ANIM_MAX) {
        anim_t *a = &g_k[s];
        a->id = (int)say_oku(&p);   if (*p++ != '|') goto bozuk;
        a->tur = *p++;              if (*p++ != '|') goto bozuk;
        a->vakit = say_oku(&p);     if (*p++ != '|') goto bozuk;
        a->tekrar = (int)say_oku(&p); if (*p++ != '|') goto bozuk;
        a->durum = (int)say_oku(&p);  if (*p++ != '|') goto bozuk;
        a->calma = (int)say_oku(&p);  if (*p++ != '|') goto bozuk;
        a->zil = say_oku(&p);       if (*p++ != '|') goto bozuk;
        { uint32_t o = 0; while (*p && *p != '\n' && o + 1 < ANIM_METIN) a->metin[o++] = *p++; a->metin[o] = 0; }
        if (a->id > 0 && (a->tur == 'O' || a->tur == 'I')) { a->dolu = 1; s++; }
        else um_bellek_doldur(a, 0, sizeof *a);
    bozuk:
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
        if (!a->dolu) um_bellek_doldur(a, 0, sizeof *a);
    }
}

static void kaydet(void) {
    static char tam[ANIM_MAX * (ANIM_METIN + 80) + 16];
    uint32_t n = 0;
    tam[0] = 0;
    for (int i = 0; i < ANIM_MAX; i++) {
        const anim_t *a = &g_k[i];
        if (!a->dolu) continue;
        char s[ANIM_METIN + 96];
        uint32_t k = um_bicim(s, sizeof s, "%d|%c|%l|%d|%d|%d|%l|%s\n", a->id, a->tur, a->vakit, a->tekrar, a->durum, a->calma, a->zil, a->metin);
        if (n + k + 1 >= sizeof tam) break;
        um_bellek_kopya(tam + n, s, k); n += k;
    }
    up_yaz_yola(yol_al(), tam, n);
}

void anim_sifirla_bellek(void) { um_bellek_doldur(g_k, 0, sizeof g_k); }

int anim_say(void) { yukle(); int n = 0; for (int i = 0; i < ANIM_MAX; i++) if (g_k[i].dolu) n++; return n; }
const anim_t *anim_al(int i) {
    yukle();
    for (int k = 0; k < ANIM_MAX; k++) if (g_k[k].dolu && i-- == 0) return &g_k[k];
    return 0;
}

int anim_ekle(char tur, int64_t vakit, int tekrar, const char *metin) {
    yukle();
    int bos = -1, enbuyuk = 0;
    for (int i = 0; i < ANIM_MAX; i++) {
        if (g_k[i].dolu) { if (g_k[i].id > enbuyuk) enbuyuk = g_k[i].id; }
        else if (bos < 0) bos = i;
    }
    if (bos < 0) return -1;
    anim_t *a = &g_k[bos];
    um_bellek_doldur(a, 0, sizeof *a);
    a->dolu = 1; a->id = enbuyuk + 1; a->tur = tur == 'I' ? 'I' : 'O';
    a->vakit = vakit; a->tekrar = tekrar;
    uint32_t o = 0;
    for (const char *p = metin ? metin : ""; *p && o + 1 < ANIM_METIN; p++)
        a->metin[o++] = (*p == '|' || *p == '\n' || *p == '\r') ? ' ' : *p;
    a->metin[o] = 0;
    kaydet();
    return a->id;
}

int anim_sil(int id) {
    yukle();
    for (int i = 0; i < ANIM_MAX; i++)
        if (g_k[i].dolu && g_k[i].id == id) { um_bellek_doldur(&g_k[i], 0, sizeof g_k[i]); kaydet(); return 1; }
    return 0;
}

static void ilerlet(anim_t *a, int64_t simdi) {
    if (a->tekrar > 0) { while (a->vakit <= simdi) a->vakit += a->tekrar; a->durum = 0; a->calma = 0; a->zil = 0; }
    else um_bellek_doldur(a, 0, sizeof *a);
}

/* ---------- vakit yoklama ---------- */
int anim_yokla(int64_t simdi, char satir[][ANIM_SATIR], int en_cok) {
    yukle();
    int say = 0, degisti = 0;
    for (int i = 0; i < ANIM_MAX && say < en_cok; i++) {
        anim_t *a = &g_k[i];
        if (!a->dolu) continue;
        char sa[24];
        if (a->tur == 'O') {
            if (a->durum != 0 || a->vakit > simdi) continue;
            saat_yaz(a->vakit, sa, sizeof sa);
            if (simdi - a->vakit > 120) um_bicim(satir[say], ANIM_SATIR, "Kaçırılan anımsatma (%s): %s", sa, a->metin);
            else um_bicim(satir[say], ANIM_SATIR, "Anımsatma: %s", a->metin);
            say++; degisti = 1;
            ilerlet(a, simdi);
        } else {
            if (a->durum == 0 && a->vakit <= simdi) {
                saat_yaz(a->vakit, sa, sizeof sa);
                a->durum = 1; a->calma = 1; a->zil = simdi + ANIM_ILAC_ARA;
                if (simdi - a->vakit > 60) um_bicim(satir[say], ANIM_SATIR, "Gecikmiş ilaç (%s): %s. İçtiysen \"içtim\" yaz.", sa, a->metin);
                else um_bicim(satir[say], ANIM_SATIR, "İlaç vakti: %s. İçtiysen \"içtim\" yaz.", a->metin);
                say++; degisti = 1;
            } else if (a->durum == 1 && a->zil <= simdi) {
                if (a->calma >= ANIM_ILAC_AZ) {
                    um_bicim(satir[say], ANIM_SATIR, "İlaç alınmadı sayıldı: %s. Sonraki vakte kadar susuyorum.", a->metin);
                    say++; degisti = 1;
                    ilerlet(a, simdi);
                } else {
                    a->calma++; a->zil = simdi + ANIM_ILAC_ARA;
                    um_bicim(satir[say], ANIM_SATIR, "İlaç vakti (%d. çağrı): %s. İçtiysen \"içtim\" yaz.", a->calma, a->metin);
                    say++; degisti = 1;
                }
            }
        }
    }
    if (degisti) kaydet();
    return say;
}

int anim_tamam(int64_t simdi) {
    yukle();
    int n = 0;
    for (int i = 0; i < ANIM_MAX; i++) {
        anim_t *a = &g_k[i];
        if (a->dolu && a->tur == 'I' && a->durum == 1) { ilerlet(a, simdi); n++; }
    }
    if (n) kaydet();
    return n;
}

int anim_ertele(int64_t simdi, int dk) {
    yukle();
    int n = 0;
    if (dk < 1) dk = 10;
    if (dk > 240) dk = 240;
    for (int i = 0; i < ANIM_MAX; i++) {
        anim_t *a = &g_k[i];
        if (a->dolu && a->tur == 'I' && a->durum == 1) { a->zil = simdi + dk; n++; }
    }
    if (n) kaydet();
    return n;
}

void anim_dizelge(char *cik, uint32_t n) {
    yukle();
    cik[0] = 0;
    int say = 0;
    for (int i = 0; i < ANIM_MAX; i++) {
        const anim_t *a = &g_k[i];
        if (!a->dolu) continue;
        char t[40], s[ANIM_METIN + 120];
        an_yaz(a->vakit, t, sizeof t);
        um_bicim(s, sizeof s, "  %d) %s — %s%s%s\n", a->id, t, a->metin,
                 a->tur == 'I' ? " (ilaç)" : "",
                 a->tekrar == 1440 ? ", her gün" : a->tekrar == 10080 ? ", her hafta" : "");
        um_ekle(cik, n, s); say++;
    }
    if (!say) um_kopya(cik, n, "  (kayıtlı anımsatıcı yok)\n");
}

/* ---------- sohbet ---------- */

/* küçült + Türkçe harfleri ASCII'ye indir (yazım kolaylığı: "içtim" = "ictim"). Boy artmaz. */
static void katla(char *c, uint32_t n, const char *s) {
    uint32_t o = 0;
    for (; *s && o + 1 < n; s++) {
        unsigned char b = (unsigned char)*s;
        if (b == 0xC3 && s[1]) {
            unsigned char k = (unsigned char)s[1]; s++;
            c[o++] = (k == 0xA7 || k == 0x87) ? 'c' : (k == 0xB6 || k == 0x96) ? 'o' : (k == 0xBC || k == 0x9C) ? 'u' : '?';
        } else if (b == 0xC4 && s[1]) {
            unsigned char k = (unsigned char)s[1]; s++;
            c[o++] = (k == 0x9F || k == 0x9E) ? 'g' : (k == 0xB1 || k == 0xB0) ? 'i' : '?';
        } else if (b == 0xC5 && s[1]) {
            unsigned char k = (unsigned char)s[1]; s++;
            c[o++] = (k == 0x9F || k == 0x9E) ? 's' : '?';
        } else c[o++] = (b >= 'A' && b <= 'Z') ? (char)(b + 32) : (char)b;
    }
    c[o] = 0;
}

typedef struct {
    int     var;
    char    tur;
    int     adet;
    int64_t vakit[4];
    int     tekrar;
    char    metin[ANIM_METIN];
} oneri_t;
static oneri_t g_oneri;

static int ic(const char *f, const char *a) { return um_icinde(f, a) != 0; }

/* m ve lk (m'nin eşbiçim küçültülmüşü) eşit uzunlukta; parçayı ikisinden birlikte siler */
static void parca_sil(char *m, char *lk, const char *parca) {
    char *p = (char *)um_icinde(lk, parca);
    if (!p) return;
    uint32_t off = (uint32_t)(p - lk), k = um_uzun(parca);
    um_bellek_kopya(m + off, m + off + k, um_uzun(m + off + k) + 1);
    um_bellek_kopya(lk + off, lk + off + k, um_uzun(lk + off + k) + 1);
}
static void sozcuk_sil(char *m, char *lk, const char *s) { for (int g = 0; g < 4 && um_icinde(lk, s); g++) parca_sil(m, lk, s); }

static void kirp(char *m) {
    char *b = m;
    while (*b == ' ' || *b == ',' || *b == ':' || *b == '-' || *b == '.') b++;
    if (b != m) um_bellek_kopya(m, b, um_uzun(b) + 1);
    uint32_t u = um_uzun(m);
    while (u && (m[u - 1] == ' ' || m[u - 1] == ',' || m[u - 1] == ':' || m[u - 1] == '-' || m[u - 1] == '.')) m[--u] = 0;
    /* art arda boşlukları tekle */
    uint32_t o = 0;
    for (uint32_t i = 0; m[i]; i++) { if (m[i] == ' ' && o && m[o - 1] == ' ') continue; m[o++] = m[i]; }
    m[o] = 0;
}

static const char *EK_SOZ[] = { "anımsatır mısın", "anımsatır mısınız", "hatırlatır mısın", "hatırlatır mısınız", "anımsat", "hatırlat",
                                "animsat", "hatirlat", "alarm kur", "alarmı kur", "alarm", "lütfen", "bana", " diye", "not al", 0 };

static int zamana_cevir(const zaman_t *z, int64_t simdi, int64_t *vakit, int *varsayilan_saat, int *gecmis) {
    int64_t bugun = gun_bol(simdi);
    int saat = z->saat, dk = z->dakika;
    *varsayilan_saat = 0; *gecmis = 0;
    if (saat < 0) { saat = 9; dk = 0; *varsayilan_saat = 1; }
    int64_t gun = bugun;
    int zorla_ileri = 0;
    if (z->gun_ofset >= 0) gun = bugun + z->gun_ofset;
    else if (z->haftagunu) {
        int bu = anim_haftagunu(simdi);
        gun = bugun + ((z->haftagunu - bu + 7) % 7);
        zorla_ileri = 1;
    } else zorla_ileri = 1;          /* yalnız saat: bugün geçtiyse yarın */
    *vakit = gun * 1440 + saat * 60 + dk;
    if (*vakit <= simdi) {
        if (zorla_ileri) { *vakit += (z->haftagunu ? 10080 : 1440); }
        else { *gecmis = 1; return 0; }
    }
    return 1;
}

static void oneri_kur_yanit(char *yanit, uint32_t n) {
    char t[40], liste[160] = "";
    if (g_oneri.tur == 'I') {
        for (int i = 0; i < g_oneri.adet; i++) {
            char sa[16]; saat_yaz(g_oneri.vakit[i], sa, sizeof sa);
            if (i) um_ekle(liste, sizeof liste, ", ");
            um_ekle(liste, sizeof liste, sa);
        }
        um_bicim(yanit, n, "İlaç alarmı kuracağım: %s — her gün %s. Vakti gelince çalar, \"içtim\" diyene dek 5 dakikada bir yineler. Kaydedeyim mi? (evet / hayır)",
                 g_oneri.metin, liste);
    } else {
        an_yaz(g_oneri.vakit[0], t, sizeof t);
        um_bicim(yanit, n, "Şunu kuracağım: %s — %s%s. Kaydedeyim mi? (evet / hayır)", t, g_oneri.metin,
                 g_oneri.tekrar == 1440 ? " (her gün)" : g_oneri.tekrar == 10080 ? " (her hafta)" : "");
    }
}

/* sıralı "SS:DD" / "SS.DD" ayıkla; metinden o parçaları sil */
static int saatleri_ayikla(char *m, char *lk, int64_t gun0, int64_t *vakit, int en_cok, int64_t simdi) {
    int adet = 0;
    for (;;) {
        char *p = lk; int bulundu = 0;
        for (; *p; p++) {
            if (*p < '0' || *p > '9' || (p > lk && p[-1] >= '0' && p[-1] <= '9')) continue;
            const char *q = p; int s = 0, d = 0, n1 = 0, n2 = 0;
            while (n1 < 2 && q[n1] >= '0' && q[n1] <= '9') { s = s * 10 + (q[n1] - '0'); n1++; }
            if (q[n1] != ':' && q[n1] != '.') continue;
            const char *r = q + n1 + 1;
            while (n2 < 2 && r[n2] >= '0' && r[n2] <= '9') { d = d * 10 + (r[n2] - '0'); n2++; }
            if (n2 != 2 || s > 23 || d > 59) continue;
            if (adet < en_cok) {
                int64_t v = gun0 * 1440 + s * 60 + d;
                if (v <= simdi) v += 1440;
                vakit[adet++] = v;
            }
            uint32_t off = (uint32_t)(p - lk), k = (uint32_t)(n1 + 1 + n2);
            um_bellek_kopya(m + off, m + off + k, um_uzun(m + off + k) + 1);
            um_bellek_kopya(lk + off, lk + off + k, um_uzun(lk + off + k) + 1);
            bulundu = 1; break;
        }
        if (!bulundu) break;
    }
    return adet;
}

int anim_konus(const char *soz, char *yanit, uint32_t n) {
    if (!soz || !*soz || !yanit || n < 8) return 0;
    char f[260], m[260], lk[260];
    katla(f, sizeof f, soz);
    uint32_t sozn = um_uzun(f);

    /* 1) bekleyen öneriye evet/hayır */
    if (g_oneri.var) {
        int evet = um_esit(f, "evet") || um_esit(f, "kaydet") || um_esit(f, "olur") || um_esit(f, "tamam") || um_esit(f, "kur") || um_esit(f, "onayliyorum") || um_esit(f, "evet kaydet");
        int hayir = um_esit(f, "hayir") || um_esit(f, "vazgec") || um_esit(f, "iptal") || um_esit(f, "kurma") || um_esit(f, "yok");
        if (evet) {
            g_oneri.var = 0;
            int ilk = -1, ok = 0;
            for (int i = 0; i < g_oneri.adet; i++) {
                int id = anim_ekle(g_oneri.tur, g_oneri.vakit[i], g_oneri.tekrar, g_oneri.metin);
                if (id > 0) { ok++; if (ilk < 0) ilk = id; }
            }
            if (!ok) um_kopya(yanit, n, "Kayıt dolu (en çok 48). Birkaç eskisini silmen gerek: \"anımsatıcılarım\" yazıp \"anımsatıcı sil 3\" de.");
            else um_bicim(yanit, n, "Kaydettim (no %d). Vakti gelince bildiririm; bunun için UMAY açık ya da \"umay --anim-bekle\" çalışıyor olmalı.", ilk);
            return 1;
        }
        if (hayir) { g_oneri.var = 0; um_kopya(yanit, n, "Peki, kurmadım."); return 1; }
        g_oneri.var = 0;                        /* başka bir şey söylendi: öneri düşer */
    }

    int64_t simdi = anim_simdi();

    /* 2) "içtim" / "tamam" — çalan ilaç var mı? */
    yukle();
    int calan = 0;
    for (int i = 0; i < ANIM_MAX; i++) if (g_k[i].dolu && g_k[i].tur == 'I' && g_k[i].durum == 1) calan++;
    if (sozn <= 32 && (ic(f, "ictim") || ic(f, "aldim") || ic(f, "yaptim") || um_esit(f, "tamam") || um_esit(f, "oldu"))) {
        if (calan) {
            int k = anim_tamam(simdi);
            um_bicim(yanit, n, "Güzel, işaretledim (%d). Sonraki vakitte yine anımsatırım.", k);
            return 1;
        }
        if (um_esit(f, "ictim")) { um_kopya(yanit, n, "Şu an çalan bir ilaç alarmı yok."); return 1; }
    }
    if (calan && ic(f, "ertele")) {
        int dk = 10;
        for (const char *p = f; *p; p++) if (*p >= '0' && *p <= '9') { dk = (int)say_oku(&p); break; }
        int k = anim_ertele(simdi, dk);
        um_bicim(yanit, n, "%d dakika sonra yine çalarım (%d alarm).", dk < 1 ? 10 : dk > 240 ? 240 : dk, k);
        return 1;
    }

    /* 3) dizelge */
    if (ic(f, "animsaticilarim") || ic(f, "alarmlarim") || ic(f, "ilaclarim") || ic(f, "randevularim") || ic(f, "yapilacaklarim") || ic(f, "animsaticilari goster")) {
        char l[2048];
        anim_dizelge(l, sizeof l);
        um_bicim(yanit, n, "Anımsatıcılarım:\n%s", l);
        return 1;
    }

    /* 4) silme */
    if (ic(f, " sil") || ic(f, "sil ") || um_esit(f, "sil")) {
        if (ic(f, "alarm") || ic(f, "animsat") || ic(f, "ilac") || ic(f, "randevu")) {
            int id = 0;
            for (const char *p = f; *p; p++) if (*p >= '0' && *p <= '9') { id = (int)say_oku(&p); break; }
            if (id > 0) {
                if (anim_sil(id)) um_bicim(yanit, n, "%d numaralı kaydı sildim.", id);
                else um_bicim(yanit, n, "%d numaralı bir kayıt bulamadım.", id);
            } else um_kopya(yanit, n, "Hangisini? Örnek: \"anımsatıcı sil 3\". Numaralar için \"anımsatıcılarım\" yaz.");
            return 1;
        }
    }

    if (simdi < 0) {
        if (ic(f, "animsat") || ic(f, "hatirlat") || ic(f, "alarm") || ic(f, "ilac")) {
            um_kopya(yanit, n, "Bu aygıtta saati okuyamıyorum; anımsatıcı kuramam.");
            return 1;
        }
        return 0;
    }

    /* 5) ilaç alarmı: "ilaç ... her gün 08:00 20:00" */
    if (ic(f, "ilac") || ic(f, "hap ")) {
        um_kopya(m, sizeof m, soz);
        um_kucult_esbicim((um_kopya(lk, sizeof lk, m), lk));
        int64_t v[4];
        int adet = saatleri_ayikla(m, lk, gun_bol(simdi), v, 4, simdi);
        if (adet > 0 && (ic(f, "her gun") || ic(f, "gunde") || ic(f, "ekle") || ic(f, "alarm") || ic(f, "animsat") || ic(f, "hatirlat") || ic(f, "ic"))) {
            static const char *AT[] = { "ilaç ekle", "ilac ekle", "ilaçlarım", "ilaclarim", "her gün", "her gun", "günde", "gunde", "alarm kur", "alarmı kur", "alarm", "ekle", "hatırlat", "anımsat", "bana", "lütfen", "içeceğim", "icecegim", "içmem gerek", "içmeliyim", "icmeliyim", " ve ", "saat ", "kez", "defa", 0 };
            for (int i = 0; AT[i]; i++) sozcuk_sil(m, lk, AT[i]);
            kirp(m);
            /* "ilaç" tek başına kalmışsa adı yine "ilaç" */
            g_oneri.var = 1; g_oneri.tur = 'I'; g_oneri.adet = adet; g_oneri.tekrar = 1440;
            for (int i = 0; i < adet; i++) g_oneri.vakit[i] = v[i];
            um_kopya(g_oneri.metin, sizeof g_oneri.metin, m[0] ? m : "ilaç");
            oneri_kur_yanit(yanit, n);
            return 1;
        }
    }

    /* 6) olay: açık bir anımsatma fiili + zaman */
    if (ic(f, "animsat") || ic(f, "hatirlat") || ic(f, "alarm") || ic(f, "randevu") || ic(f, "not al")) {
        int tekrar = 0;
        um_kopya(m, sizeof m, soz);
        um_kopya(lk, sizeof lk, m); um_kucult_esbicim(lk);
        if (ic(f, "her gun")) { tekrar = 1440; sozcuk_sil(m, lk, "her gün"); sozcuk_sil(m, lk, "her gun"); }
        else if (ic(f, "her hafta")) { tekrar = 10080; sozcuk_sil(m, lk, "her hafta"); }
        else if (ic(f, "her pazartesi") || ic(f, "her sali") || ic(f, "her carsamba") || ic(f, "her persembe") || ic(f, "her cuma") || ic(f, "her cumartesi") || ic(f, "her pazar")) {
            tekrar = 10080; sozcuk_sil(m, lk, "her ");
        }
        zaman_t z;
        if (!zaman_ayikla(m, &z)) return 0;
        char is[ANIM_METIN];
        um_kopya(is, sizeof is, z.is);
        char il[ANIM_METIN];
        um_kopya(il, sizeof il, is); um_kucult_esbicim(il);
        for (int i = 0; EK_SOZ[i]; i++) sozcuk_sil(is, il, EK_SOZ[i]);
        kirp(is);
        int64_t vakit; int vs, gecmis;
        if (!zamana_cevir(&z, simdi, &vakit, &vs, &gecmis)) {
            um_kopya(yanit, n, "O saat geçmiş görünüyor. Yarın ya da başka bir gün diyebilir misin?");
            return 1;
        }
        if (tekrar == 1440 && z.gun_ofset < 0 && !z.haftagunu) { /* "her gün 09:00" — sıradaki gün/saat */ }
        g_oneri.var = 1; g_oneri.tur = 'O'; g_oneri.adet = 1; g_oneri.vakit[0] = vakit; g_oneri.tekrar = tekrar;
        um_kopya(g_oneri.metin, sizeof g_oneri.metin, is[0] ? is : "(adsız anımsatma)");
        oneri_kur_yanit(yanit, n);
        if (vs) um_ekle(yanit, n, " (Saat söylemediğin için 9.00 aldım.)");
        return 1;
    }
    return 0;
}

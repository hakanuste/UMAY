/* GİZLİLİK DUVARI — bkz. umay_gizli.h
 *
 * BU DOSYADA İKİ ÖLÇÜLMÜŞ AÇIK KAPATILDI:
 *
 * 1) UZUN METİN SESSİZCE GEÇİYORDU. Tarama, metni `static char k[2048]`
 *    arabelleğine katlıyordu; gerisi kesiliyor ve hiç bakılmıyordu.
 *    Ölçüldü: 1900 bayt + "tapu" -> yakalanıyor (1);
 *             2300 bayt + "tapu" -> YAKALANMIYOR (0).
 *    Yanıtlar ve notlar bu sınırı kolayca geçer, yani duvar tam da
 *    korunması gereken yerde açılıyordu. Artık metin PENCERE PENCERE
 *    taranır, uzunluk sınırı yoktur.
 *
 * 2) DURAAĞAN ARABELLEK KOŞUT ÇAĞRIDA YARIŞIYORDU. `k`, `g` ve `c`
 *    duraağandı; iki iş parçası aynı anda girince birbirinin
 *    arabelleğini bozuyordu. Yanlış yanıt veren bir gizlilik denetimi
 *    AÇIK TARAFA düşer: gitmemesi gereken istek gider. Artık bütün
 *    tarama arabellekleri YIĞINDADIR.
 *
 * PENCERE SINIRINDA SÖZCÜK KAÇMAZ: her turda önceki pencerenin son
 * GZ_BINDIRME baytı yeni pencerenin başına taşınır. Bindirme, en uzun
 * aranan sözcükten (GZ_SOZ-1) kısa olamaz; yoksa sınırı iki pencereye
 * bölünen sözcük kaçar. Sınaması yazıldı: sözcük tam sınıra denk
 * getirilip yoklanır.
 *
 * ADRES ile METİN AYNI YOLDAN GEÇER: adreste yüzde çözümü (%XX) ve '+'
 * boşluğa çevirme vardır, gerisi birebir aynıdır. Eskiden adres ayrı
 * bir duraağan arabellekte çözülüp sonra gizli_mi'ye veriliyordu, yani
 * adres de 2048 baytta kesiliyordu. Artık ikisi de aynı akış
 * taramasını kullanır.
 */
#include "umay_gizli.h"
#include "umay_str.h"
#include "umay_port.h"
#include "umay_ortak.h"

#define GZ_MAX       512u     /* kullanıcı listesindeki en çok sözcük   */
#define GZ_SOZ        48u     /* bir sözcüğün katlanmış en çok boyu     */
#define GZ_PENCERE  1024u     /* yığındaki tarama penceresi             */
#define GZ_BINDIRME (GZ_SOZ - 1u)   /* 47: en uzun sözcük kadar taşıma  */

/* ---------------------------------------------------------------- */
/* harf katlama                                                      */
/* ---------------------------------------------------------------- */
/* Türkçe harfi ASCII'ye indirir: ç->c ğ->g ı/İ->i ö->o ş->s ü->u.
 * Böylece "Koçarlı", "KOCARLI" ve "kocarli" aynı sayılır. 0 dönerse
 * bu iki bayt bilinen bir harf değildir. */
static char ikili(int c, int d) {
    if (c == 0xC3) {
        if (d == 0xA7 || d == 0x87) return 'c';
        if (d == 0xB6 || d == 0x96) return 'o';
        if (d == 0xBC || d == 0x9C) return 'u';
        if (d == 0xA2 || d == 0x82) return 'a';
        if (d == 0xAE || d == 0x8E) return 'i';
        if (d == 0xBB || d == 0x9B) return 'u';
    } else if (c == 0xC4) {
        if (d == 0x9F || d == 0x9E) return 'g';
        if (d == 0xB1 || d == 0xB0) return 'i';
    } else if (c == 0xC5) {
        if (d == 0x9F || d == 0x9E) return 's';
    }
    return 0;
}

/* Tüm diziyi katlar (kısa, sınırı belli yerler için: liste satırı,
 * gömülü sözcük). Akış taraması bunu kullanmaz. */
static uint32_t katla(char *hedef, uint32_t n, const char *kaynak) {
    uint32_t o = 0;
    const unsigned char *p = (const unsigned char *)kaynak;
    while (*p && o + 1 < n) {
        unsigned char c = *p;
        if ((c == 0xC3 || c == 0xC4 || c == 0xC5) && p[1]) {
            char r = ikili(c, p[1]);
            if (r) { hedef[o++] = r; p += 2; continue; }
        }
        hedef[o++] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : (char)c;
        p++;
    }
    hedef[o] = 0;
    return o;
}

/* ---------------------------------------------------------------- */
/* akış: metin ya da adres, bayt bayt                                */
/* ---------------------------------------------------------------- */
static int onaltilik(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

typedef struct {
    const char *p;
    int coz;        /* 1 = adres: %XX çözülür, '+' boşluk olur */
    int geri;       /* geri itilmiş bayt; yoksa -1            */
} akis_t;

static int ak_al(akis_t *a) {
    if (a->geri >= 0) { int b = a->geri; a->geri = -1; return b; }
    unsigned char c = (unsigned char)*a->p;
    if (!c) return -1;
    a->p++;
    if (a->coz) {
        if (c == '+') return ' ';
        if (c == '%') {
            int y = onaltilik(a->p[0]);
            int d = (y >= 0) ? onaltilik(a->p[1]) : -1;
            if (y >= 0 && d >= 0) { a->p += 2; return y * 16 + d; }
        }
    }
    return c;
}

/* Tek baytlık geri itme yeter: katlama en çok bir bayt ileri bakar. */
static void ak_geri(akis_t *a, int b) { a->geri = b; }

/* Akıştan en çok n-1 katlanmış bayt üretir. Dönen değer üretilen
 * bayttır; 0 ise akış bitmiştir. Çok baytlı diziyi ortadan bölmez:
 * ikinci baytı okumak için ak_al kullanılır, tanınmazsa geri itilir. */
static uint32_t katla_akis(char *hedef, uint32_t n, akis_t *a) {
    uint32_t o = 0;
    while (o + 1 < n) {
        int c = ak_al(a);
        if (c < 0) break;
        if (c == 0xC3 || c == 0xC4 || c == 0xC5) {
            int d = ak_al(a);
            if (d < 0) { hedef[o++] = (char)c; break; }
            char r = ikili(c, d);
            if (r) { hedef[o++] = r; continue; }
            ak_geri(a, d);
        }
        hedef[o++] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : (char)c;
    }
    hedef[o] = 0;
    return o;
}

/* ---------------------------------------------------------------- */
/* sözcük listeleri                                                  */
/* ---------------------------------------------------------------- */
static const char *GOMULU[] = {
    "şifre", "parola", "iban", "tapu", "parsel", "maaş", "miras", "hisseli", 0
};

/* ÇİFT ARABELLEK — liste yenilenirken okuyan iş parçası BOZUK ya da
 * BOŞ liste görmesin.
 *
 * Eski kurulumda yenileme önce `g_kul_n = 0` yapıyordu: o anda tarayan
 * bir iş parçası listeyi BOŞ görür ve gitmemesi gereken istek gider —
 * yine açık tarafa düşme. Şimdi yeni liste KULLANILMAYAN arabelleğe
 * yazılır, sonra işaretçi ve sayı yayımlanır. Okuyan ya eski ya yeni
 * listeyi görür; yarısını hiç görmez.
 *
 * Kilit kullanılmıyor: port katmanında kilit yok ve duvar ÖTÜKEN'de de
 * koşar. Hizalı işaretçi ataması hedef yapılarda tek yazmadır. Kalan
 * kuramsal pencere: işaretçiyi alıp sayıyı yeni okuyan bir iş parçası
 * — bu durumda da okuduğu satırlar geçerlidir, yalnız eski olabilir. */
static char   g_a[GZ_MAX][GZ_SOZ];
static char   g_b[GZ_MAX][GZ_SOZ];
static char (*g_kul)[GZ_SOZ] = g_a;
static int    g_kul_n;
static char   g_yuklu[200];
static int    g_yuklendi;

int gizli_yukle(const char *kut) {
    char (*yeni)[GZ_SOZ] = (g_kul == g_a) ? g_b : g_a;   /* boş olan */
    int n = 0;

    um_kopya(g_yuklu, sizeof g_yuklu, kut ? kut : "");
    if (!kut) { g_kul = yeni; g_kul_n = 0; g_yuklendi = 1; return 0; }

    char yol[320];
    um_bicim(yol, sizeof yol, "%s/gizli.txt", kut);
    int64_t boy = up_boy(yol);
    if (boy <= 0 || boy > 131072) { g_kul = yeni; g_kul_n = 0; g_yuklendi = 1; return 0; }
    char *m = (char *)up_ayir((uint64_t)boy + 1);
    if (!m) { g_yuklendi = 1; return 0; }        /* eski liste YERİNDE kalır */
    up_tut_t t = up_ac(yol, 0);
    int64_t o = t == UP_GECERSIZ ? -1 : up_oku(t, m, (uint64_t)boy);
    if (t != UP_GECERSIZ) up_kapat(t);
    if (o <= 0) { up_biraktir(m); g_yuklendi = 1; return 0; }
    m[o] = 0;

    char *p = m;
    while (*p && n < (int)GZ_MAX) {
        char *s = p;
        while (*p && *p != '\n') p++;
        if (*p) *p++ = 0;
        while (*s == ' ' || *s == '\t') s++;
        uint32_t u = um_uzun(s);
        while (u && (s[u - 1] == ' ' || s[u - 1] == '\r' || s[u - 1] == '\t')) s[--u] = 0;
        if (!*s || *s == '#' || u < 3) continue;   /* 3 harften kısası çok yanlış eşleşir */
        katla(yeni[n], GZ_SOZ, s);
        n++;
    }
    up_biraktir(m);

    /* YAYIM SIRASI: satırlar yazıldı, şimdi işaretçi, en sonunda sayı. */
    g_kul = yeni;
    g_kul_n = n;
    g_yuklendi = 1;
    return n;
}

static void liste_hazir(void) {
    const char *kok = ayar_metin("kutuphane", "kut");
    if (!g_yuklendi || !um_esit(g_yuklu, kok)) gizli_yukle(kok);
}

/* ---------------------------------------------------------------- */
/* tarama                                                            */
/* ---------------------------------------------------------------- */
/* Katlanmış pencerede sözcük arar. Arabellek YIĞINDA: koşut çağrıda
 * yarışmaz. */
static int igneler(const char *pencere, char (*kul)[GZ_SOZ], int kul_n) {
    char g[GZ_SOZ];
    for (int i = 0; GOMULU[i]; i++) {
        katla(g, sizeof g, GOMULU[i]);
        if (um_icinde(pencere, g)) return 1;
    }
    for (int i = 0; i < kul_n; i++)
        if (kul[i][0] && um_icinde(pencere, kul[i])) return 1;
    return 0;
}

/* Akışı pencere pencere tarar. Uzunluk sınırı YOKTUR.
 *
 * Her turda pencerenin son GZ_BINDIRME baytı başa taşınır, böylece
 * sınırı iki pencereye bölünen sözcük kaçmaz. İlerleme kesindir:
 * pencere GZ_PENCERE, taşıma en çok GZ_BINDIRME olduğundan her tur en
 * az GZ_PENCERE - GZ_BINDIRME - 1 yeni bayt tüketilir. */
static int tara(akis_t *a) {
    char pen[GZ_PENCERE];
    char (*kul)[GZ_SOZ] = g_kul;      /* bir kez al: tarama boyunca sabit */
    int   kul_n = g_kul_n;
    uint32_t tas = 0;

    for (;;) {
        uint32_t u = katla_akis(pen + tas, GZ_PENCERE - tas, a);
        uint32_t o = tas + u;
        if (!o) return 0;
        if (igneler(pen, kul, kul_n)) return 1;
        if (!u) return 0;                           /* akış bitti */
        uint32_t b = (o > GZ_BINDIRME) ? GZ_BINDIRME : o;
        um_bellek_kopya(pen, pen + o - b, b);       /* ileri kopya: örtüşmede güvenli */
        pen[b] = 0;
        tas = b;
    }
}

int gizli_mi(const char *metin) {
    if (!metin || !*metin) return 0;
    liste_hazir();
    akis_t a = { metin, 0, -1 };
    return tara(&a);
}

int gizli_adres_mi(const char *url) {
    if (!url || !*url) return 0;
    liste_hazir();
    akis_t a = { url, 1, -1 };
    return tara(&a);
}

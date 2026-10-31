/* ULAK canlı veri — bkz. umay_ulak.h */
#include "umay_ulak.h"
#include "umay_ogren.h"
#include "umay_gizli.h"
#include "umay_str.h"
#include "umay_port.h"
#include "umay_ortak.h"

#define UL_YANIT (1u << 16)

/* "43.1234" -> "43,1234" */
static void virgul(char *s) { for (; *s; s++) if (*s == '.') *s = ','; }

/* blok içinde <ad>değer</ad> okur */
static int xml_deger(const char *p, const char *son, const char *ad, char *cik, uint32_t n) {
    char ac[40]; um_bicim(ac, sizeof ac, "<%s>", ad);
    const char *a = um_icinde(p, ac);
    if (!a || a >= son) return 0;
    a += um_uzun(ac);
    uint32_t o = 0;
    while (a < son && *a != '<' && o + 1 < n) cik[o++] = *a++;
    cik[o] = 0;
    return o > 0;
}

static int kur_sorusu(const char *k, const char **kod, const char **ad) {
    static const struct { const char *anahtar, *kod, *ad; } T[] = {
        {"dolar", "USD", "ABD doları"}, {"euro", "EUR", "Euro"},
        {"sterlin", "GBP", "İngiliz sterlini"}, {"pound", "GBP", "İngiliz sterlini"},
        {"isviçre frangı", "CHF", "İsviçre frangı"}, {"frank", "CHF", "İsviçre frangı"},
        {"yen", "JPY", "Japon yeni (100)"}, {0,0,0}
    };
    int ipucu = um_icinde(k, "kaç") || um_icinde(k, "kur") || um_icinde(k, "ne kadar") || um_icinde(k, "tl")
             || um_icinde(k, "lira") || um_icinde(k, "fiyat") || um_icinde(k, "değer") || um_icinde(k, "döviz");
    if (!ipucu) return 0;
    for (int i = 0; T[i].anahtar; i++)
        if (um_icinde(k, T[i].anahtar)) { *kod = T[i].kod; *ad = T[i].ad; return 1; }
    if (um_icinde(k, "döviz")) { *kod = "USD"; *ad = "ABD doları"; return 1; }
    return 0;
}

static int kur_getir(const char *kod, const char *ad, baglam_t *b) {
    char *x = (char *)up_ayir(UL_YANIT);
    if (!x) return 0;
    int ok = 0;
    const char *url = up_ortam("UMAY_AG_TCMB");
    if (!url || !*url) url = "https://www.tcmb.gov.tr/kurlar/today.xml";
    int64_t r = ogren_ag_getir(url, x, UL_YANIT);
    if (r > 0) {
        x[r < UL_YANIT ? r : UL_YANIT - 1] = 0;
        char anahtar[24]; um_bicim(anahtar, sizeof anahtar, "Kod=\"%s\"", kod);
        const char *c = um_icinde(x, anahtar);
        if (c) {
            const char *son = um_icinde(c, "</Currency>");
            if (!son) son = c + um_uzun(c);
            char al[32], sat[32], tarih[24] = "bugün";
            if (xml_deger(c, son, "ForexBuying", al, sizeof al) && xml_deger(c, son, "ForexSelling", sat, sizeof sat)) {
                const char *t = um_icinde(x, "Tarih=\"");
                if (t) { t += 7; uint32_t o = 0; while (*t && *t != '"' && o + 1 < sizeof tarih) tarih[o++] = *t++; tarih[o] = 0; }
                virgul(al); virgul(sat);
                um_bicim(b->alinti, sizeof b->alinti,
                         "%s için TCMB günlük kuru (%s): döviz alış %s TL, döviz satış %s TL. "
                         "Bu, Merkez Bankası'nın ilan ettiği kurdur; bankaların ve kuyumcuların anlık fiyatı farklı olabilir.",
                         ad, tarih, al, sat);
                um_kopya(b->baslik, sizeof b->baslik, "ULAK canlı veri: döviz kuru");
                um_kopya(b->kaynak, sizeof b->kaynak, "TCMB günlük kurlar (tcmb.gov.tr)");
                ok = 1;
            }
        }
    }
    up_biraktir(x);
    return ok;
}

/* ------------------------------------------------------------ hava */
static const char *hava_adi(int k) {
    if (k == 0) return "açık";
    if (k <= 3) return "parçalı bulutlu";
    if (k == 45 || k == 48) return "sisli";
    if (k >= 51 && k <= 57) return "çiseleyen yağmurlu";
    if (k >= 61 && k <= 67) return "yağmurlu";
    if (k >= 71 && k <= 77) return "karlı";
    if (k >= 80 && k <= 82) return "sağanak yağışlı";
    if (k >= 95) return "gök gürültülü fırtınalı";
    return "değişken";
}

static int hava_sorusu(const char *k) {
    return um_icinde(k, "hava durumu") || um_icinde(k, "hava kaç") || um_icinde(k, "hava nasıl")
        || um_icinde(k, "hava sıcak") || um_icinde(k, "hava soğuk") || um_icinde(k, "yağmur yağacak")
        || um_icinde(k, "sıcaklık kaç") || um_icinde(k, "kaç derece");
}

/* "aydın'da hava durumu" / "aydında hava nasıl" -> "aydın" ; bulunamazsa ayar/varsayılan */
static void konum_ayikla(const char *k, char *cik, uint32_t n) {
    /* Varsayılan konum YOK: yer adı soruda açıkça yoksa hava durumu sorulmaz
     * (kullanıcının evi gibi bir yeri kendiliğinden dışarı yollamamak için). */
    cik[0] = 0;
    const char *h = um_icinde(k, "hava");
    if (!h) h = um_icinde(k, "kaç derece");
    if (!h || h == k) return;
    const char *e = h - 1;
    while (e > k && *e == ' ') e--;
    const char *s = e;
    while (s > k && *(s - 1) != ' ') s--;
    char t[48]; uint32_t o = 0;
    for (const char *p = s; p <= e && o + 1 < sizeof t; p++) t[o++] = *p;
    t[o] = 0;
    /* ek at: ' işaretinden sonrası, ya da -da/-de/-ta/-te */
    for (uint32_t i = 0; t[i]; i++) if (t[i] == '\'' ) { t[i] = 0; break; }
    uint32_t L = um_uzun(t);
    if (L > 5 && t[L-1] == 'e' && (t[L-2] == 'd' || t[L-2] == 't')) t[L-2] = 0;
    else if (L > 5 && t[L-1] == 'a' && (t[L-2] == 'd' || t[L-2] == 't')) t[L-2] = 0;
    if (um_esit(t, "bugün") || um_esit(t, "yarın") || um_esit(t, "şimdi") || um_esit(t, "an") ||
        um_esit(t, "şu") || um_esit(t, "bu") || um_esit(t, "dışarıda") || um_esit(t, "bugünkü") || !t[0]) return;
    um_kopya(cik, n, t);
}

/* "anahtar":değer sayısı (nesne içinde), metin olarak */
static int json_sayi(const char *p, const char *anahtar, char *cik, uint32_t n) {
    char a[48]; um_bicim(a, sizeof a, "\"%s\":", anahtar);
    const char *s = um_icinde(p, a);
    if (!s) return 0;
    s += um_uzun(a);
    uint32_t o = 0;
    while (*s && *s != ',' && *s != '}' && o + 1 < n) cik[o++] = *s++;
    cik[o] = 0;
    return o > 0;
}

static int hava_getir(const char *yer, baglam_t *b) {
    char *x = (char *)up_ayir(UL_YANIT);
    if (!x) return 0;
    int ok = 0;
    char kodlu[160], url[512];
    ogren_url_kodla(yer, kodlu, sizeof kodlu);
    const char *g = up_ortam("UMAY_AG_GEOKOD");
    um_bicim(url, sizeof url, "%s?name=%s&count=1&language=tr&format=json",
             (g && *g) ? g : "https://geocoding-api.open-meteo.com/v1/search", kodlu);
    int64_t r = ogren_ag_getir(url, x, UL_YANIT);
    if (r > 0) {
        x[r < UL_YANIT ? r : UL_YANIT - 1] = 0;
        char enlem[32], boylam[32], ad[64];
        if (json_sayi(x, "latitude", enlem, sizeof enlem) && json_sayi(x, "longitude", boylam, sizeof boylam)) {
            if (ogren_json_dizge(x, "name", ad, sizeof ad) <= 0) um_kopya(ad, sizeof ad, yer);
            const char *h = up_ortam("UMAY_AG_HAVA");
            um_bicim(url, sizeof url,
                     "%s?latitude=%s&longitude=%s&current=temperature_2m,relative_humidity_2m,wind_speed_10m,precipitation,weather_code&timezone=auto",
                     (h && *h) ? h : "https://api.open-meteo.com/v1/forecast", enlem, boylam);
            r = ogren_ag_getir(url, x, UL_YANIT);
            if (r > 0) {
                x[r < UL_YANIT ? r : UL_YANIT - 1] = 0;
                const char *cur = um_icinde(x, "\"current\":{");
                char sic[24], nem[24], ruz[24], yag[24], kod[16], zaman[32] = "";
                if (cur && json_sayi(cur, "temperature_2m", sic, sizeof sic) && json_sayi(cur, "weather_code", kod, sizeof kod)) {
                    if (!json_sayi(cur, "relative_humidity_2m", nem, sizeof nem)) um_kopya(nem, sizeof nem, "?");
                    if (!json_sayi(cur, "wind_speed_10m", ruz, sizeof ruz)) um_kopya(ruz, sizeof ruz, "?");
                    if (!json_sayi(cur, "precipitation", yag, sizeof yag)) um_kopya(yag, sizeof yag, "0");
                    ogren_json_dizge(cur, "time", zaman, sizeof zaman);
                    int wk = 0; for (const char *c = kod; *c >= '0' && *c <= '9'; c++) wk = wk * 10 + (*c - '0');
                    virgul(sic); virgul(nem); virgul(ruz); virgul(yag);
                    um_bicim(b->alinti, sizeof b->alinti,
                             "%s için anlık hava durumu (%s): %s, sıcaklık %s °C, nem %% %s, rüzgâr %s km/sa, yağış %s mm.",
                             ad, zaman[0] ? zaman : "şimdi", hava_adi(wk), sic, nem, ruz, yag);
                    um_kopya(b->baslik, sizeof b->baslik, "ULAK canlı veri: hava durumu");
                    um_kopya(b->kaynak, sizeof b->kaynak, "Open-Meteo (open-meteo.com)");
                    ok = 1;
                }
            }
        }
    }
    up_biraktir(x);
    return ok;
}

int ulak_canli(const char *soru, baglam_t *b) {
    if (!soru || !*soru || !b || !ogren_acik()) return 0;
    char k[320];
    um_kopya(k, sizeof k, soru);
    um_kucult_esbicim(k);
    um_bellek_doldur(b, 0, sizeof *b);
    um_kopya(b->kategori, sizeof b->kategori, "ortak");
    um_kopya(b->dil, sizeof b->dil, "tr");
    b->kaynak_tur = KT_DIS;
    b->skor = 99;
    const char *kod, *ad;
    if (kur_sorusu(k, &kod, &ad)) return kur_getir(kod, ad, b);
    if (hava_sorusu(k)) {
        char yer[64]; konum_ayikla(k, yer, sizeof yer);
        if (!yer[0] || gizli_mi(yer)) return 0;
        return hava_getir(yer, b);
    }
    return 0;
}

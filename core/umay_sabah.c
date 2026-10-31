/* SABAH — bkz. umay_sabah.h. Yalnız um_* / up_* kullanır; kesirli sayı (float) yok: ÖTÜKEN'de de derlenir. */
#include "umay_sabah.h"
#include "umay_anim.h"
#include "umay_ceviri.h"
#include "umay_ogren.h"
#include "umay_port.h"
#include "umay_str.h"

#define SB_ARA (1u << 19)
static char g_kay_yol[320], g_port_yol[320];
void sabah_dosya_ata(const char *kaynaklar, const char *portfoy) {
    g_kay_yol[0] = 0; g_port_yol[0] = 0;
    if (kaynaklar) um_kopya(g_kay_yol, sizeof g_kay_yol, kaynaklar);
    if (portfoy) um_kopya(g_port_yol, sizeof g_port_yol, portfoy);
}          /* tek yanıt tamponu (RSS dosyaları 100-300 KB olabilir) */

const char *sabah_varsayilan_kaynaklar(void) {
    return
"# SABAH BİLDİRİSİ KAYNAKLARI — satır: haber|bölüm|ad (tür)|dil|adres|adet|süzgeç\n"
"# süzgeç (isteğe bağlı): virgülle ayrılmış sözcükler; yalnız başlığında geçenler alınır.\n"
"# kur|KOD,KOD,…   stooq|SEMBOL|ad   yahoo|SEMBOL|ad\n"
"kur|USD,EUR,GBP,RUB,CNY\n"
"# yahoo|SEMBOL|ad|BÖLÜM[|gram]  — gün/hafta/ay/yıl değişimi. ^ işareti otomatik kodlanır. gram: ons fiyatından gram TL hesaplar.\n"
"yahoo|GC=F|Altın (ons, ABD doları, vadeli)|ALTIN, GÜMÜŞ VE ENERJİ|gram\n"
"yahoo|SI=F|Gümüş (ons, ABD doları, vadeli)|ALTIN, GÜMÜŞ VE ENERJİ|gram\n"
"yahoo|BZ=F|Brent petrol (varil, ABD doları)|ALTIN, GÜMÜŞ VE ENERJİ\n"
"yahoo|CL=F|WTI petrol (varil, ABD doları)|ALTIN, GÜMÜŞ VE ENERJİ\n"
"yahoo|NG=F|Doğal gaz (ABD doları)|ALTIN, GÜMÜŞ VE ENERJİ\n"
"yahoo|HG=F|Bakır (libre, ABD doları)|ALTIN, GÜMÜŞ VE ENERJİ\n"
"yahoo|XU100.IS|Borsa İstanbul BIST 100|DÜNYA BORSALARI\n"
"yahoo|^GSPC|ABD S&P 500|DÜNYA BORSALARI\n"
"yahoo|^IXIC|ABD Nasdaq|DÜNYA BORSALARI\n"
"yahoo|^DJI|ABD Dow Jones|DÜNYA BORSALARI\n"
"yahoo|^GDAXI|Almanya DAX|DÜNYA BORSALARI\n"
"yahoo|^FCHI|Fransa CAC 40|DÜNYA BORSALARI\n"
"yahoo|^FTSE|İngiltere FTSE 100|DÜNYA BORSALARI\n"
"yahoo|^N225|Japonya Nikkei 225|DÜNYA BORSALARI\n"
"yahoo|^HSI|Hong Kong Hang Seng|DÜNYA BORSALARI\n"
"yahoo|000001.SS|Çin Şanghay Bileşik|DÜNYA BORSALARI\n"
"haber|Piyasalar (yabancı finans basını)|CNBC (ABD, ticari yayıncı)|en|https://www.cnbc.com/id/100003114/device/rss/rss.html|4\n"
"haber|Piyasalar (yabancı finans basını)|Bloomberg Piyasalar (ABD, ticari yayıncı)|en|https://feeds.bloomberg.com/markets/news.rss|4\n"
"haber|Piyasalar (yabancı finans basını)|Investing.com Haberler|en|https://www.investing.com/rss/news.rss|4\n"
"haber|Emtia haberleri ve analiz başlıkları|CNBC (emtia süzgeci)|en|https://www.cnbc.com/id/100003114/device/rss/rss.html|3|gold,silver,oil,crude,brent,copper,gas,commodit,metal\n"
"haber|Emtia haberleri ve analiz başlıkları|Bloomberg (emtia süzgeci)|en|https://feeds.bloomberg.com/markets/news.rss|3|gold,silver,oil,crude,brent,copper,gas,commodit,metal\n"
"haber|Türkiye ekonomisi (yabancı basın)|BBC İş Dünyası (kamu yayıncısı)|en|http://feeds.bbci.co.uk/news/business/rss.xml|3|turk,lira,istanbul,erdogan,erdoğan,ankara\n"
"haber|Türkiye ekonomisi (yabancı basın)|The Guardian İş Dünyası (bağımsız)|en|https://www.theguardian.com/business/rss|3|turk,lira,istanbul,erdogan,erdoğan,ankara\n"
"haber|Almanya|Tagesschau (kamu yayıncısı)|de|https://www.tagesschau.de/infoservices/alle-meldungen-100~rss2.xml|4\n"
"haber|Almanya|Der Spiegel (bağımsız)|de|https://www.spiegel.de/schlagzeilen/index.rss|3\n"
"haber|Fransa|Le Monde (bağımsız)|fr|https://www.lemonde.fr/rss/une.xml|4\n"
"haber|Fransa|France 24 (kamu fonlu)|fr|https://www.france24.com/fr/rss|3\n"
"haber|İngiltere|BBC Dünya (kamu yayıncısı)|en|http://feeds.bbci.co.uk/news/world/rss.xml|4\n"
"haber|İngiltere|The Guardian Dünya (bağımsız)|en|https://www.theguardian.com/world/rss|3\n"
"haber|Japonya|NHK Dış Haberler (kamu yayıncısı)|ja|https://www3.nhk.or.jp/rss/news/cat6.xml|4\n"
"haber|Japonya|The Japan Times (bağımsız)|en|https://www.japantimes.co.jp/feed/|3\n"
"haber|Rusya|TASS (devlet ajansı)|ru|https://tass.ru/rss/v2.xml|3\n"
"haber|Rusya|Meduza (bağımsız, yurt dışından)|ru|https://meduza.io/rss/all|3\n"
"haber|Sağlık (Dünya Sağlık Örgütü)|DSÖ salgın bildirimleri|en|https://www.who.int/feeds/entity/csr/don/en/rss.xml|3\n";
}

int sabah_sorusu(const char *soz) {
    char f[200];
    anim_katla(f, sizeof f, soz ? soz : "");
    if (um_uzun(f) > 90) return 0;
    if (um_icinde(f, "her sabah") || um_icinde(f, "her gun") || um_icinde(f, "animsat")) return 0;   /* zamanlama isteği anim'e gider */
    return um_icinde(f, "sabah bildiri") || um_icinde(f, "gunun bildiri") || um_icinde(f, "sabah rapor")
        || um_icinde(f, "gunluk bulten") || um_icinde(f, "sabah bulten") || um_icinde(f, "piyasa ozeti");
}

/* ---------- sayı: 4 ondalık ölçekli tamsayı ---------- */
static int sayi_oku(const char *s, int64_t *v) {
    int64_t t = 0, o = 0; int eksi = 0, g = 0, nokta = 0, ond = 0;
    if (*s == '-') { eksi = 1; s++; }
    for (; (*s >= '0' && *s <= '9') || ((*s == '.' || *s == ',') && !nokta); s++) {
        if (*s == '.' || *s == ',') { nokta = 1; continue; }
        if (!nokta) t = t * 10 + (*s - '0');
        else if (ond < 4) { o = o * 10 + (*s - '0'); ond++; }
        g = 1;
    }
    if (!g) return 0;
    while (ond < 4) { o *= 10; ond++; }
    *v = (eksi ? -1 : 1) * (t * 10000 + o);
    return 1;
}
/* 1234567 (=123,4567) -> "123,46" ; binlik ayracı nokta */
static void sayi_yaz(int64_t v, int ond, char *c, uint32_t n) {
    char t[40]; uint32_t o = 0;
    int eksi = v < 0; if (eksi) v = -v;
    int64_t tam = v / 10000, kes = v % 10000;
    char r[24]; int rn = 0;
    if (!tam) r[rn++] = '0';
    while (tam) { r[rn++] = (char)('0' + tam % 10); tam /= 10; }
    if (eksi) t[o++] = '-';
    for (int i = rn - 1; i >= 0; i--) { t[o++] = r[i]; if (i && i % 3 == 0) t[o++] = '.'; }
    if (ond > 0) {
        char k[8]; int64_t q = kes;
        for (int i = 3; i >= 0; i--) { k[i] = (char)('0' + q % 10); q /= 10; }
        /* yuvarlama basit: kesme (okunurluk için yeterli) */
        t[o++] = ',';
        for (int i = 0; i < ond && i < 4; i++) t[o++] = k[i];
    }
    t[o] = 0;
    um_kopya(c, n, t);
}
static void yuzde_yaz(int64_t simdi, int64_t onceki, char *c, uint32_t n) {
    if (onceki <= 0) { um_kopya(c, n, "?"); return; }
    int64_t bp = (simdi - onceki) * 10000 / onceki;      /* yüzde ×100 */
    int eksi = bp < 0; if (eksi) bp = -bp;
    um_bicim(c, n, "%c%%%d,%c%d", eksi ? '-' : '+', (int)(bp / 100), (char)('0' + (bp % 100) / 10), (int)(bp % 10));
}

static int al(const char *url, char *x) {
    int64_t r = ogren_ag_getir(url, x, SB_ARA);
    if (r <= 0) return 0;
    x[r < SB_ARA ? r : SB_ARA - 1] = 0;
    return 1;
}

/* ---------- kur ---------- */
static int64_t g_usd_sat;            /* gram altın hesabı için (4 ondalık ölçekli) */

static const char *kur_ad(const char *kod) {
    if (um_esit(kod, "USD")) return "Dolar";
    if (um_esit(kod, "EUR")) return "Euro";
    if (um_esit(kod, "GBP")) return "Sterlin";
    if (um_esit(kod, "RUB")) return "Ruble";
    if (um_esit(kod, "CNY")) return "Yuan";
    if (um_esit(kod, "JPY")) return "Yen (100)";
    if (um_esit(kod, "CHF")) return "İsviçre frangı";
    return kod;
}

static void kurlar(const char *kodlar, char *x, char *cik, uint32_t n) {
    const char *url = up_ortam("UMAY_AG_TCMB");
    if (!url || !*url) url = "https://www.tcmb.gov.tr/kurlar/today.xml";
    um_ekle(cik, n, "PARA — Merkez Bankası günlük kurları\n");
    if (!al(url, x)) { um_ekle(cik, n, "  alınamadı (ağ yok ya da TCMB'ye ulaşılamadı)\n\n"); return; }
    const char *t = um_icinde(x, "Tarih=\"");
    char tarih[24] = "";
    if (t) { t += 7; uint32_t o = 0; while (*t && *t != '"' && o + 1 < sizeof tarih) tarih[o++] = *t++; tarih[o] = 0; }
    char kod[8]; uint32_t ko = 0;
    for (const char *p = kodlar;; p++) {
        if (*p == ',' || *p == 0) {
            kod[ko] = 0; ko = 0;
            if (kod[0]) {
                char anahtar[24]; um_bicim(anahtar, sizeof anahtar, "Kod=\"%s\"", kod);
                const char *c = um_icinde(x, anahtar);
                const char *son = c ? um_icinde(c, "</Currency>") : 0;
                char a[32] = "", s[32] = "", sat_ham[32] = "";
                int ok = 0;
                if (c && son) {
                    const char *p1 = um_icinde(c, "<ForexBuying>"), *p2 = um_icinde(c, "<ForexSelling>");
                    if (p1 && p1 < son && p2 && p2 < son) {
                        p1 += 13; p2 += 14;
                        uint32_t o = 0; while (p1 < son && *p1 != '<' && o + 1 < sizeof a) a[o++] = *p1++; a[o] = 0;
                        o = 0; while (p2 < son && *p2 != '<' && o + 1 < sizeof s) s[o++] = *p2++; s[o] = 0;
                        um_kopya(sat_ham, sizeof sat_ham, s);
                        ok = a[0] && s[0];
                    }
                }
                char sat[96];
                if (ok) {
                    for (char *q = a; *q; q++) if (*q == '.') *q = ',';
                    for (char *q = s; *q; q++) if (*q == '.') *q = ',';
                    um_bicim(sat, sizeof sat, "  %s: alış %s · satış %s TL\n", kur_ad(kod), a, s);
                    if (um_esit(kod, "USD")) sayi_oku(sat_ham, &g_usd_sat);
                } else um_bicim(sat, sizeof sat, "  %s: alınamadı\n", kur_ad(kod));
                um_ekle(cik, n, sat);
            }
            if (!*p) break;
        } else if (ko + 1 < sizeof kod) kod[ko++] = *p;
    }
    if (tarih[0]) { char s[64]; um_bicim(s, sizeof s, "  (TCMB, %s; bankaların anlık kuru farklı olabilir)\n", tarih); um_ekle(cik, n, s); }
    um_ekle(cik, n, "\n");
}

/* ---------- altın / gümüş (stooq CSV) ---------- */
static int stooq(const char *sembol, char *x, int64_t *kapanis, int64_t *acilis) {
    char url[160];
    um_bicim(url, sizeof url, "https://stooq.com/q/l/?s=%s&f=sd2t2ohlcv&h&e=csv", sembol);
    if (!al(url, x)) return 0;
    const char *p = um_icinde(x, "\n");
    if (!p) return 0;
    p++;
    /* Sembol,Tarih,Saat,Açılış,Yüksek,Düşük,Kapanış,Hacim */
    int alan = 0; const char *a[8] = {0};
    a[0] = p;
    for (; *p && *p != '\n' && *p != '\r'; p++) if (*p == ',' && alan < 7) a[++alan] = p + 1;
    if (alan < 6) return 0;
    return sayi_oku(a[3], acilis) && sayi_oku(a[6], kapanis);
}

static void metal(const char *sembol, const char *ad, char *x, char *cik, uint32_t n) {
    int64_t k, o;
    char s[200];
    if (!stooq(sembol, x, &k, &o)) { um_bicim(s, sizeof s, "  %s: alınamadı\n", ad); um_ekle(cik, n, s); return; }
    char kt[40], yz[24];
    sayi_yaz(k, 2, kt, sizeof kt); yuzde_yaz(k, o, yz, sizeof yz);
    um_bicim(s, sizeof s, "  %s: %s (gün içi %s)\n", ad, kt, yz);
    um_ekle(cik, n, s);
    if (g_usd_sat > 0) {
        int64_t gram = k * g_usd_sat / 311035;          /* ons -> gram, TL (4 ondalık ölçekli) */
        char gt[40]; sayi_yaz(gram, 2, gt, sizeof gt);
        um_bicim(s, sizeof s, "    ≈ gram fiyatı (hesaplanan): %s TL\n", gt);
        um_ekle(cik, n, s);
    }
}

static uint32_t bol(const char *s, char ayrac, char alan[][400], uint32_t en_cok);
static int json_sayi(const char *x, const char *anahtar, int64_t *v) {
    char a[48]; um_bicim(a, sizeof a, "\"%s\":", anahtar);
    const char *p = um_icinde(x, a);
    if (!p) return 0;
    return sayi_oku(p + um_uzun(a), v);
}

/* ---------- yahoo grafik uç noktası: gün / hafta / ay / yıl ---------- */
#define YH_N 400
static int64_t g_kap[YH_N];

/* "close":[ ... ] dizisini okur (null atlanır); dönen: değer sayısı */
static int yahoo_kapanislar(const char *x) {
    const char *p = um_icinde(x, "\"close\":[");
    if (!p) return 0;
    p += 9;
    int n = 0;
    while (*p && *p != ']') {
        if (um_bas_mi(p, "null")) { p += 4; }
        else {
            int64_t v;
            if (sayi_oku(p, &v)) {
                if (n < YH_N) g_kap[n++] = v; else { um_bellek_kopya(g_kap, g_kap + 1, (YH_N - 1) * sizeof g_kap[0]); g_kap[YH_N - 1] = v; }
                while ((*p >= '0' && *p <= '9') || *p == '.' || *p == '-' || *p == 'e' || *p == 'E' || *p == '+') p++;
            } else p++;
        }
        while (*p == ',' || *p == ' ') p++;
    }
    return n;
}

static void yahoo_url(const char *sembol, char *url, uint32_t n) {
    char kod[96]; uint32_t o = 0;
    for (const char *p = sembol; *p && o + 4 < sizeof kod; p++) {
        if (*p == '^') { kod[o++] = '%'; kod[o++] = '5'; kod[o++] = 'E'; } else kod[o++] = *p;
    }
    kod[o] = 0;
    um_bicim(url, n, "https://query1.finance.yahoo.com/v8/finance/chart/%s?range=1y&interval=1d", kod);
}

/* Bir varlığı okur. Dönen: 0 = alınamadı. */
static int yahoo_oku(const char *sembol, char *x, int64_t *son, int64_t *gun, int64_t *hafta, int64_t *ay, int64_t *yil) {
    char url[200];
    yahoo_url(sembol, url, sizeof url);
    if (!al(url, x)) return 0;
    int k = yahoo_kapanislar(x);
    if (k < 2) {
        int64_t v;
        if (!json_sayi(x, "regularMarketPrice", &v)) return 0;
        *son = v; *gun = *hafta = *ay = *yil = 0;
        return 1;
    }
    *son = g_kap[k - 1];
    *gun = g_kap[k - 2];
    *hafta = k > 5 ? g_kap[k - 6] : 0;
    *ay = k > 21 ? g_kap[k - 22] : 0;
    *yil = g_kap[0];
    return 1;
}

static void ufuklar(int64_t son, int64_t gun, int64_t hafta, int64_t ay, int64_t yil, char *c, uint32_t n) {
    char a[24], b[24], d[24], e[24];
    yuzde_yaz(son, gun, a, sizeof a);
    if (hafta) yuzde_yaz(son, hafta, b, sizeof b); else um_kopya(b, sizeof b, "?");
    if (ay) yuzde_yaz(son, ay, d, sizeof d); else um_kopya(d, sizeof d, "?");
    if (yil) yuzde_yaz(son, yil, e, sizeof e); else um_kopya(e, sizeof e, "?");
    um_bicim(c, n, "gün %s · hafta %s · ay %s · yıl %s", gun ? a : "?", b, d, e);
}

static void yahoo_satiri(const char *sembol, const char *ad, int gram, char *x, char *cik, uint32_t n) {
    int64_t son, g, h, m, y;
    char s[300];
    if (!yahoo_oku(sembol, x, &son, &g, &h, &m, &y)) { um_bicim(s, sizeof s, "  %s: alınamadı\n", ad); um_ekle(cik, n, s); return; }
    char kt[40], uf[120];
    sayi_yaz(son, 2, kt, sizeof kt);
    ufuklar(son, g, h, m, y, uf, sizeof uf);
    um_bicim(s, sizeof s, "  %s: %s\n    %s\n", ad, kt, uf);
    um_ekle(cik, n, s);
    if (gram && g_usd_sat > 0) {
        char gt[40]; sayi_yaz(son * g_usd_sat / 311035, 2, gt, sizeof gt);
        um_bicim(s, sizeof s, "    ≈ gram: %s TL (ons × dolar satış ÷ 31,1035; kuyumcu fiyatı değildir)\n", gt);
        um_ekle(cik, n, s);
    }
}

/* ---------- senin varlıkların (yerel portfoy.txt) ---------- */
/* satır: varlık|SEMBOL|ad|adet|para   (adet boş olabilir). Yalnız SEMBOL ağa gider; adet cihazdan çıkmaz. */
static void portfoy(char *x, char *cik, uint32_t n) {
    char yol[320]; if (g_port_yol[0]) um_kopya(yol, sizeof yol, g_port_yol); else up_kut_dosya("portfoy.txt", yol, sizeof yol);
    static char dosya[4096];
    up_tut_t t = up_ac(yol, UP_OKU);
    if (t == UP_GECERSIZ) return;
    uint32_t d = 0;
    for (;;) { int64_t k = up_oku(t, dosya + d, sizeof dosya - 1 - d); if (k <= 0) break; d += (uint32_t)k; if (d + 1 >= sizeof dosya) break; }
    up_kapat(t);
    dosya[d] = 0;
    char alan[8][400];
    int basladi = 0;
    for (const char *p = dosya; *p;) {
        const char *satir = p;
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
        if (*satir == '#' || *satir == '\n' || *satir == '\r') continue;
        uint32_t k = bol(satir, '|', alan, 8);
        if (k < 3) continue;
        if (!basladi) { um_ekle(cik, n, "SENİN VARLIKLARIN (yerel portfoy.txt)\n"); basladi = 1; }
        int64_t son, g, h, m, y, adet = 0;
        char s[360];
        if (!yahoo_oku(alan[1], x, &son, &g, &h, &m, &y)) { um_bicim(s, sizeof s, "  %s: alınamadı\n", alan[2]); um_ekle(cik, n, s); continue; }
        char kt[40], uf[120];
        sayi_yaz(son, 2, kt, sizeof kt); ufuklar(son, g, h, m, y, uf, sizeof uf);
        um_bicim(s, sizeof s, "  %s: %s %s\n    %s\n", alan[2], kt, k >= 5 ? alan[4] : "", uf);
        um_ekle(cik, n, s);
        if (k >= 4 && sayi_oku(alan[3], &adet) && adet > 0) {
            int64_t deger = son * adet / 10000, fark = (son - g) * adet / 10000;
            char dt[40], ft[40];
            sayi_yaz(deger, 2, dt, sizeof dt); sayi_yaz(fark < 0 ? -fark : fark, 2, ft, sizeof ft);
            um_bicim(s, sizeof s, "    elindeki değer ≈ %s %s (son gün %s%s)\n", dt, k >= 5 ? alan[4] : "", fark < 0 ? "-" : "+", ft);
            um_ekle(cik, n, s);
        }
    }
    if (basladi) um_ekle(cik, n, "\n");
}

/* ---------- RSS / Atom başlıkları ---------- */
static void ent_coz(char *m) {
    char *o = m;
    for (char *p = m; *p;) {
        if (*p == '&') {
            if (um_bas_mi(p, "&amp;")) { *o++ = '&'; p += 5; continue; }
            if (um_bas_mi(p, "&lt;")) { *o++ = '<'; p += 4; continue; }
            if (um_bas_mi(p, "&gt;")) { *o++ = '>'; p += 4; continue; }
            if (um_bas_mi(p, "&quot;")) { *o++ = '"'; p += 6; continue; }
            if (um_bas_mi(p, "&apos;")) { *o++ = '\''; p += 6; continue; }
            if (p[1] == '#') {
                const char *q = p + 2; uint32_t kn = 0; int hex = 0;
                if (*q == 'x' || *q == 'X') { hex = 1; q++; }
                int g = 0;
                for (; (*q >= '0' && *q <= '9') || (hex && ((*q >= 'a' && *q <= 'f') || (*q >= 'A' && *q <= 'F'))); q++) {
                    int d = (*q <= '9') ? *q - '0' : ((*q | 32) - 'a' + 10);
                    kn = kn * (hex ? 16 : 10) + (uint32_t)d; g = 1;
                }
                if (g && *q == ';' && kn) { char b[8]; uint32_t k = um_utf8_yaz(kn, b); for (uint32_t i = 0; i < k; i++) *o++ = b[i]; p = (char *)q + 1; continue; }
            }
        }
        *o++ = *p++;
    }
    *o = 0;
}

/* bloktan (b..son) başlığı alır; yoksa 0 */
static int baslik_al(const char *b, const char *son, char *c, uint32_t n) {
    const char *t = um_icinde(b, "<title");
    if (!t || t >= son) return 0;
    while (t < son && *t != '>') t++;
    if (t >= son) return 0;
    t++;
    if (um_bas_mi(t, "<![CDATA[")) t += 9;
    uint32_t o = 0; int etiket = 0;
    while (t < son && o + 1 < n) {
        if (um_bas_mi(t, "]]>")) break;
        if (um_bas_mi(t, "</title")) break;
        if (*t == '<') etiket = 1;
        if (!etiket) c[o++] = (*t == '\n' || *t == '\r' || *t == '\t') ? ' ' : *t;
        if (*t == '>') etiket = 0;
        t++;
    }
    c[o] = 0;
    ent_coz(c);
    /* baş/son boşluk, UTF-8 bütünlüğü */
    char *bs = c; while (*bs == ' ') bs++;
    if (bs != c) um_bellek_kopya(c, bs, um_uzun(bs) + 1);
    uint32_t u = um_uzun(c);
    while (u && c[u - 1] == ' ') c[--u] = 0;
    return u > 3;
}

static int suzgec_uyar(const char *baslik, const char *suzgec) {
    if (!suzgec || !*suzgec) return 1;
    char k[300]; anim_katla(k, sizeof k, baslik);      /* İngilizce "Istanbul" Türkçe küçültmede bozulmasın: ASCII katlama */
    char sz[200]; anim_katla(sz, sizeof sz, suzgec);
    char s[40]; uint32_t so = 0;
    for (const char *p = sz;; p++) {
        if (*p == ',' || *p == 0) {
            s[so] = 0; so = 0;
            if (s[0] && um_icinde(k, s)) return 1;
            if (!*p) break;
        } else if (so + 1 < sizeof s) s[so++] = *p;
    }
    return 0;
}

static int g_ceviri_say, g_ceviri_yok;
static ceviri_yontem_t g_yontem;

static void haber_kaynagi(const char *bolum, const char *ad, const char *dil, const char *url, int adet, const char *suzgec,
                          char *x, char *cik, uint32_t n, const char **onceki_bolum, char *alinamayan, uint32_t an) {
    if (!al(url, x)) {
        if (alinamayan[0]) um_ekle(alinamayan, an, ", ");
        um_ekle(alinamayan, an, ad);
        return;
    }
    if (!*onceki_bolum || !um_esit(*onceki_bolum, bolum)) {      /* başlık yalnız en az bir kaynak geldiyse yazılır */
        char s[160]; um_bicim(s, sizeof s, "%s\n", bolum);
        um_ekle(cik, n, "\n"); um_ekle(cik, n, s);
        *onceki_bolum = bolum;
    }
    int bulunan = 0, tarandi = 0;
    const char *p = x;
    char satir[800];
    um_bicim(satir, sizeof satir, "  %s:\n", ad);
    int baslik_yazildi = 0;
    while (bulunan < adet && tarandi < 60) {
        const char *i1 = um_icinde(p, "<item"), *i2 = um_icinde(p, "<entry");
        const char *b = !i1 ? i2 : (!i2 ? i1 : (i1 < i2 ? i1 : i2));
        if (!b) break;
        const char *son = um_icinde(b + 5, b == i1 ? "</item>" : "</entry>");
        if (!son) son = b + um_uzun(b);
        char bas[320];
        tarandi++;
        p = son;
        if (!baslik_al(b, son, bas, sizeof bas)) continue;
        if (um_uzun(bas) > 230) { uint32_t k = 230; while (k && (((unsigned char)bas[k]) & 0xC0) == 0x80) k--; bas[k] = 0; }
        if (!suzgec_uyar(bas, suzgec)) continue;
        char tr[700]; ceviri_yontem_t y = CY_YOK;
        int cv = um_esit(dil, "tr") ? 0 : ceviri_yap(dil, "tr", bas, tr, sizeof tr, &y);
        if (um_esit(dil, "tr")) um_kopya(tr, sizeof tr, bas);
        else if (cv <= 0) { um_bicim(tr, sizeof tr, "(çevrilemedi) %s", bas); g_ceviri_yok++; }
        else { g_ceviri_say++; g_yontem = y; }
        if (!baslik_yazildi) { um_ekle(cik, n, satir); baslik_yazildi = 1; }
        char s[800]; um_bicim(s, sizeof s, "   • %s\n", tr);
        um_ekle(cik, n, s);
        bulunan++;
    }
    if (!bulunan) {
        if (!baslik_yazildi) { um_ekle(cik, n, satir); }
        um_ekle(cik, n, suzgec && *suzgec ? "   • (bugün Türkiye ile ilgili başlık bulunamadı)\n" : "   • (başlık okunamadı)\n");
    }
}

/* ---------- kaynak listesi ---------- */
static uint32_t bol(const char *s, char ayrac, char alan[][400], uint32_t en_cok) {
    uint32_t k = 0, o = 0;
    for (;; s++) {
        if (*s == ayrac || *s == 0 || *s == '\n' || *s == '\r') {
            alan[k][o] = 0; k++; o = 0;
            if (k >= en_cok || *s == 0 || *s == '\n' || *s == '\r') break;
        } else if (o + 1 < 400) alan[k][o++] = *s;
    }
    return k;
}

uint32_t sabah_bildirisi(char *cik, uint32_t n) {
    if (!cik || n < 64) return 0;
    cik[0] = 0;
    if (!ogren_acik()) {
        um_kopya(cik, n, "Sabah bildirisi için ağa çıkmam gerek, ama \"internetten öğren\" kapalı (UMAY_OGREN=0). Ayarlardan açarsan hazırlarım.");
        return um_uzun(cik);
    }
    char *x = (char *)up_ayir(SB_ARA);
    if (!x) { um_kopya(cik, n, "Bellek yetmedi."); return um_uzun(cik); }

    /* kaynak listesi */
    static char dosya[8192];
    const char *liste = sabah_varsayilan_kaynaklar();
    {
        char yol[320]; if (g_kay_yol[0]) um_kopya(yol, sizeof yol, g_kay_yol); else up_kut_dosya("haberler.txt", yol, sizeof yol);
        up_tut_t t = up_ac(yol, UP_OKU);
        if (t != UP_GECERSIZ) {
            uint32_t d = 0;
            for (;;) { int64_t k = up_oku(t, dosya + d, sizeof dosya - 1 - d); if (k <= 0) break; d += (uint32_t)k; if (d + 1 >= sizeof dosya) break; }
            up_kapat(t);
            dosya[d] = 0;
            if (d > 10) liste = dosya;
        }
    }

    g_usd_sat = 0; g_ceviri_say = 0; g_ceviri_yok = 0; g_yontem = CY_YOK;
    int64_t simdi = anim_simdi();
    if (simdi >= 0) {
        int y, m, d, sa, mi; char tt[80];
        anim_ayir(simdi, &y, &m, &d, &sa, &mi);
        static const char *AY[] = { "Ocak", "Şubat", "Mart", "Nisan", "Mayıs", "Haziran", "Temmuz", "Ağustos", "Eylül", "Ekim", "Kasım", "Aralık" };
        static const char *GN[] = { "Pazartesi", "Salı", "Çarşamba", "Perşembe", "Cuma", "Cumartesi", "Pazar" };
        um_bicim(tt, sizeof tt, "SABAH BİLDİRİSİ — %d %s %d %s\n\n", d, AY[m - 1], y, GN[anim_haftagunu(simdi) - 1]);
        um_ekle(cik, n, tt);
    } else um_ekle(cik, n, "SABAH BİLDİRİSİ\n\n");

    /* 1. geçiş: kur önce (gram altın ona dayanır); sonra metal, borsa; sonra haberler */
    char alan[8][400];
    uint32_t bolum_para = 0;
    const char *p = liste;
    while (*p) {
        const char *satir = p;
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
        if (*satir == '#' || *satir == '\n' || *satir == '\r') continue;
        uint32_t k = bol(satir, '|', alan, 8);
        if (k >= 2 && um_esit(alan[0], "kur")) { kurlar(alan[1], x, cik, n); bolum_para = 1; }
    }
    (void)bolum_para;
    {
        char bolum_onceki[160] = "";
        p = liste;
        while (*p) {
            const char *satir = p;
            while (*p && *p != '\n') p++;
            if (*p == '\n') p++;
            if (*satir == '#' || *satir == '\n' || *satir == '\r') continue;
            uint32_t k = bol(satir, '|', alan, 8);
            if (k >= 4 && um_esit(alan[0], "yahoo")) {
                if (!um_esit(bolum_onceki, alan[3])) {
                    if (bolum_onceki[0]) um_ekle(cik, n, "\n");
                    um_ekle(cik, n, alan[3]); um_ekle(cik, n, "\n");
                    um_kopya(bolum_onceki, sizeof bolum_onceki, alan[3]);
                }
                yahoo_satiri(alan[1], alan[2], k >= 5 && um_esit(alan[4], "gram"), x, cik, n);
            } else if (k >= 3 && um_esit(alan[0], "stooq")) {
                if (!um_esit(bolum_onceki, "ALTIN VE GÜMÜŞ")) {
                    if (bolum_onceki[0]) um_ekle(cik, n, "\n");
                    um_ekle(cik, n, "ALTIN VE GÜMÜŞ\n"); um_kopya(bolum_onceki, sizeof bolum_onceki, "ALTIN VE GÜMÜŞ");
                }
                metal(alan[1], alan[2], x, cik, n);
            }
        }
        if (bolum_onceki[0]) um_ekle(cik, n, "\n");
    }
    portfoy(x, cik, n);

    const char *onceki = 0;
    static char alinamayan[1000];
    alinamayan[0] = 0;
    char bolumler[10][160]; int nb = 0;
    p = liste;
    while (*p) {
        const char *satir = p;
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
        if (*satir == '#' || *satir == '\n' || *satir == '\r') continue;
        uint32_t k = bol(satir, '|', alan, 8);
        if (k >= 6 && um_esit(alan[0], "haber")) {
            /* bölüm adı kalıcı olsun (onceki işaretçisi alan[] içine bakar) */
            int ix = -1;
            for (int i = 0; i < nb; i++) if (um_esit(bolumler[i], alan[1])) ix = i;
            if (ix < 0 && nb < 10) { um_kopya(bolumler[nb], sizeof bolumler[nb], alan[1]); ix = nb++; }
            const char *bol_adi = ix >= 0 ? bolumler[ix] : alan[1];
            int adet = 3; { int64_t v; if (sayi_oku(alan[5], &v) && v >= 10000) adet = (int)(v / 10000); }
            if (adet > 8) adet = 8;
            haber_kaynagi(bol_adi, alan[2], alan[3], alan[4], adet, k >= 7 ? alan[6] : "", x, cik, n, &onceki, alinamayan, sizeof alinamayan);
        }
    }
    up_biraktir(x);

    um_ekle(cik, n, "\n");
    if (alinamayan[0]) { char s[1100]; um_bicim(s, sizeof s, "Alınamayan kaynaklar: %s\n", alinamayan); um_ekle(cik, n, s); }
    {
        char s[400];
        um_bicim(s, sizeof s, "Not: Başlıklar makine çevirisidir (%s; doğrulanmamış)%s. Kamu yayıncısı ile bağımsız yayını yan yana okuyup karşılaştır; "
                 "başlık yalnız haberin kapısıdır.\n",
                 g_ceviri_say ? ceviri_yontem_adi(g_yontem) : "çeviri yok",
                 g_ceviri_yok ? ", bazıları çevrilemedi" : "");
        um_ekle(cik, n, s);
    }
    return um_uzun(cik);
}

/* =====================================================================
 * UMAY — PIPER ses arka ucu (yalnız ana bilgisayar yapımı)
 *
 * NE YAPAR: UMAY'ın yanıtını Piper ile seslendirir. Piper, sinir ağı
 * tabanlı (VITS) bir konuşma sentezleyicisidir; CPU'da gerçek zamanlı
 * koşar, çevrim dışıdır ve Türkçe sesleri vardır.
 *
 * NEDEN AYRI DOSYA: ses, UMAY'ın çekirdeğine gömülmez. Burası PORT
 * katmanının ana bilgisayar tarafıdır; ÖTÜKEN yapımına hiç girmez
 * (ÖTÜKEN yapımına hiç girmez).
 *
 * NEDEN PIPER: ÖTÜKEN'in kendi sentezleyicisi (cekirdek/src/dede.c)
 * formant sentezidir. Ölçülebilir her niteliği hedefe oturtuldu —
 * perde, formantlar, tayf dengesi, ünsüz ayrımı, hece vurgusu — ama
 * sözcükler yine net değil. Formant sentezinin tavanı budur: belirli
 * bir insanın sesini taşıyamaz ve doğal konuşmanın ince yapısını
 * kurallarla yeniden üretmek yılların işidir. Piper o yapıyı binlerce
 * saat konuşmadan öğrenmiştir.
 *
 * SAHTE BAŞARI YOK: Piper ya da ses dosyası ya da çalıcı yoksa bu
 * arka uç AÇILMAZ ve -1 döner. UMAY o zaman yalnız yazıyla yanıtlar.
 * Metni günlüğe basıp "konuştum" demek yankıdan başka bir şey değildir.
 *
 * İKİ YOL
 * 1. PIPER İKİLİSİ — "piper" çalıştırılabiliri yoldaysa kullanılır.
 * 2. DOĞRUDAN ONNX — ikili yoksa, model ses/soylet.py ile koşulur:
 *       metin --espeak-ng--> IPA fonemleri --eşleme--> ONNX --> dalga
 *    Gereken: python3, onnxruntime, espeak-ng, .onnx ve .onnx.json.
 *    Piper ikilisi her dağıtımda paketli değil; model dosyası ise tek
 *    başına yeterlidir. Bu yol onu yeterli kılar.
 *
 * KURULUM: ses/piper_kur.sh
 * AYARLAR (ortam değişkenleri):
 *   UMAY_PIPER        piper çalıştırılabiliri (varsayılan: yoldaki "piper")
 *   UMAY_PIPER_SES    .onnx ses modeli
 *   UMAY_PIPER_AYAR   .onnx.json (verilmezse <model>.json denenir)
 *   UMAY_PIPER_PY     soylet.py yolu
 *   UMAY_PIPER_UZUN   konuşma ağırlığı (length_scale); Dede Korkut 1.52
 *   UMAY_PIPER_RENK   ses rengi düzeltmesi (öntanımlı KAPALI; bkz. ara())
 *   UMAY_SES_OYNAT    çalıcı komutu (varsayılan: bulunan ilki)
 * ===================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "umay_host.h"
#include "umay_piper.h"

#define YOL_AZAMI 512
#define G_YERLESIK "@yerlesik"   /* platformun kendi WAV calicisi */

typedef enum { YOL_YOK = 0, YOL_IKILI, YOL_ONNX } piper_yol_t;

static int  g_bakildi;          /* arama bir kez yapılır */
static int  g_hazir;
static piper_yol_t g_yol;
static char g_piper[YOL_AZAMI];
static char g_ses[YOL_AZAMI];
static char g_ayar[YOL_AZAMI];
static char g_py[YOL_AZAMI];
static char g_oynat[YOL_AZAMI];
/* KONUSMA AGIRLIGI (Piper length_scale)
 * 1.6, Soylev kaydinin susma payini (%25.9) tam tutuyordu ama kulakta
 * fazla yavas kaldi; kullanici %5 hizlandirmayi istedi: 1.6 / 1.05.
 * Buyuk deger = yavas. UMAY_PIPER_UZUN ile degistirilebilir. */
static char g_uzun[32] = "1.52";
static char g_renk[YOL_AZAMI];   /* ses rengi duzeltmesi (istege bagli) */

static int dosya_var(const char *y)
{
    struct stat s;
    return y && *y && stat(y, &s) == 0 && S_ISREG(s.st_mode);
}

/* Çalıştırılabilir PATH içinde var mı? (platforma göre ':' / ';' ve .exe
 * ayrımı uh_yol_ara'dadır.) */
static int komut_var(const char *ad)
{
    return uh_yol_ara(ad, 0, 0);
}

static void varsayilan_ses_yolu(char *c, size_t n)
{
    const char *ev = uh_ev();
    if (!ev) ev = ".";
    snprintf(c, n, "%s/.local/share/umay/ses/tr_TR-fahrettin-medium.onnx", ev);
}

/* soylet.py aranir: ayar, kurulum oneki, kaynak agaci. */
static int soylet_bul(char *c, size_t n)
{
    const char *v = getenv("UMAY_PIPER_PY");
    if (v && *v) { snprintf(c, n, "%s", v); return dosya_var(c); }
    const char *ev = uh_ev();
#ifdef UMAY_KUT_ONEK
    snprintf(c, n, "%s/ses/soylet.py", UMAY_KUT_ONEK);
    if (dosya_var(c)) return 1;
#endif
    if (ev) {
        snprintf(c, n, "%s/.local/share/umay/ses/soylet.py", ev);
        if (dosya_var(c)) return 1;
    }
    snprintf(c, n, "ses/soylet.py");
    return dosya_var(c);
}

/* <model>.onnx -> <model>.onnx.json, olmazsa <model>.json */
/* <model>.onnx -> <model>.onnx.json, olmazsa <model>.json
 *
 * YOL KIRPILMASI: snprintf sigmazsa SESSIZCE kirpar ve geriye gecerli
 * gorunen ama yanlis bir yol kalir. ".json" eklemesi tamponu asiyorsa
 * basarisiz olunur — kirpilmis yolla dosya aramak, var olan bir ayari
 * "yok" saymak ya da baska bir dosyayi acmak demektir.
 * -Werror=format-truncation yakaladi. */
static int ek_ile(char *c, size_t n, const char *kok, size_t kok_uz,
                  const char *ek)
{
    if (kok_uz + strlen(ek) + 1 > n) return 0;      /* sigmaz */
    memcpy(c, kok, kok_uz);
    memcpy(c + kok_uz, ek, strlen(ek) + 1);
    return 1;
}

static int ayar_bul(const char *model, char *c, size_t n)
{
    const char *v = getenv("UMAY_PIPER_AYAR");
    if (v && *v) {
        if (strlen(v) + 1 > n) return 0;
        snprintf(c, n, "%s", v);
        return dosya_var(c);
    }
    size_t L = strlen(model);
    if (ek_ile(c, n, model, L, ".json") && dosya_var(c)) return 1;
    if (L > 5 && strcmp(model + L - 5, ".onnx") == 0 &&
        ek_ile(c, n, model, L - 5, ".json") && dosya_var(c)) return 1;
    c[0] = 0;
    return 0;
}

/* Python'un adi: Linux/macOS/Android'de python3; Windows'ta cogunlukla
 * yalniz "python" vardir. Bulunan ilki kullanilir. */
static char g_python[32];
static const char *python_adi(void)
{
    if (!g_python[0]) {
        if (komut_var("python3")) snprintf(g_python, sizeof g_python, "python3");
        else if (komut_var("python")) snprintf(g_python, sizeof g_python, "python");
    }
    return g_python[0] ? g_python : 0;
}

/* python + onnxruntime GERCEKTEN var mi? Yalniz dosya varligina
 * bakmak yetmez: onnxruntime kurulu degilse betik her cagrida
 * patlar ve surucu "hazir" gorunur. Sahte basari yok. */
static int onnx_kosulabilir(void)
{
    const char *py = python_adi();
    if (!py || !komut_var("espeak-ng")) return 0;
    char kod[64], komut[256];
    uh_tirnak("import onnxruntime", kod, sizeof kod);
    snprintf(komut, sizeof komut, "%s -c %s %s", py, kod, uh_sessiz());
    return uh_komut(komut) == 0;
}

static void ara(void)
{
    if (g_bakildi) return;
    g_bakildi = 1;
    g_hazir = 0;
    g_yol = YOL_YOK;

    const char *u = getenv("UMAY_PIPER_UZUN");
    if (u && *u) snprintf(g_uzun, sizeof g_uzun, "%s", u);

    /* SES RENGI — ÖNTANIMLI OLARAK KAPALI, istege bagli.
     *
     * NEDEN KAPALI: duzeltme, sesi Soylev kaydinin OLCULEN tayf
     * dengesine oturtuyor ve bunu basariyla yapiyor — ortalama sapma
     * 4.08 dB'den 0.82 dB'ye iniyor, perde 97 Hz'den 111 Hz'e cikiyor
     * (hedef 109). Ama DINLEYEN ham sesi secti.
     *
     * Ders: olcute yakinlik, kulaga iyilik DEGILDIR. Olculen sey
     * "referans kayda benzerlik"ti; istenen sey ise iyi bir Dede
     * Korkut sesiydi. Ikisi ayni cikmadi. Arac ve olculen duzeltme
     * yerinde duruyor (arac/ses_rengi.py, ses/dede_renk.json) ama
     * yalnizca ACIKCA istenirse uygulanir:
     *     UMAY_PIPER_RENK=<yol>/dede_renk.json
     * Ham Piper sesi: perde ~97 Hz, derin; UMAY_PIPER_UZUN=1.52. */
    const char *r = getenv("UMAY_PIPER_RENK");
    if (r && *r && dosya_var(r)) snprintf(g_renk, sizeof g_renk, "%s", r);

    /* 1) ses modeli — iki yolun ikisi de buna muhtac */
    const char *m = getenv("UMAY_PIPER_SES");
    if (m && *m) snprintf(g_ses, sizeof g_ses, "%s", m);
    else varsayilan_ses_yolu(g_ses, sizeof g_ses);
    if (!dosya_var(g_ses)) return;

    /* 2) yol secimi: once piper ikilisi, yoksa dogrudan ONNX */
    const char *p = getenv("UMAY_PIPER");
    if (p && *p) {
        if (dosya_var(p) || komut_var(p)) {
            snprintf(g_piper, sizeof g_piper, "%s", p);
            g_yol = YOL_IKILI;
        }
    } else if (komut_var("piper")) {
        snprintf(g_piper, sizeof g_piper, "piper");
        g_yol = YOL_IKILI;
    }
    if (g_yol == YOL_YOK) {
        if (soylet_bul(g_py, sizeof g_py) &&
            ayar_bul(g_ses, g_ayar, sizeof g_ayar) &&
            onnx_kosulabilir())
            g_yol = YOL_ONNX;
    }
    if (g_yol == YOL_YOK) return;

    /* 3) çalıcı */
    const char *o = getenv("UMAY_SES_OYNAT");
    if (o && *o) {
        /* Elle verilen çalıcı DA doğrulanır. Doğrulanmadığında sürücü
         * takılıyor, "dede-korkut takıldı" yazıyor ama hiçbir zaman
         * ses çıkmıyordu: sahte başarının ta kendisi. Sınamada
         * yakalandı. Komutun ilk sözcüğüne bakılır (gerisi bayrak). */
        char ilk[YOL_AZAMI];
        size_t n = 0;
        while (o[n] && o[n] != ' ' && n + 1 < sizeof ilk) { ilk[n] = o[n]; n++; }
        ilk[n] = 0;
        if (!dosya_var(ilk) && !komut_var(ilk)) return;
        snprintf(g_oynat, sizeof g_oynat, "%s", o);
    } else {
        static const char *adaylar[] = { "paplay", "aplay", "afplay",
                                         "play", "ffplay",
                                         "termux-media-player", 0 };
        int i;
        for (i = 0; adaylar[i]; i++)
            if (komut_var(adaylar[i])) break;
        if (adaylar[i]) {
            if (strcmp(adaylar[i], "ffplay") == 0)
                snprintf(g_oynat, sizeof g_oynat, "ffplay -autoexit -nodisp -loglevel quiet");
            else if (strcmp(adaylar[i], "termux-media-player") == 0)
                snprintf(g_oynat, sizeof g_oynat, "termux-media-player play");
            else
                snprintf(g_oynat, sizeof g_oynat, "%s", adaylar[i]);
        } else if (uh_yerlesik_oynatici()) {
            snprintf(g_oynat, sizeof g_oynat, "%s", G_YERLESIK);   /* Windows */
        } else {
            return;
        }
    }

    g_hazir = 1;
}

int piper_var(void)
{
    ara();
    return g_hazir;
}

const char *piper_ses_adi(void)
{
    ara();
    return g_hazir ? g_ses : "yok";
}

/* Hangi yol kullaniliyor? Gunluge ELLE "piper" yazmak yerine bu
 * sorulur: yol degisince kayit yaniltici olmasin. */
const char *piper_yol_adi(void)
{
    ara();
    switch (g_yol) {
    case YOL_IKILI: return "piper ikilisi";
    case YOL_ONNX:  return "dogrudan onnx";
    default:        return "yok";
    }
}

int piper_konus(const char *metin)
{
    if (!metin || !*metin) return -1;
    ara();
    if (!g_hazir) return -1;

    char ad[64], wav[YOL_AZAMI];
    snprintf(ad, sizeof ad, "umay-dede-%d.wav", uh_pid());
    uh_gecici(ad, wav, sizeof wav);

    char t_wav[YOL_AZAMI * 2], t_ses[YOL_AZAMI * 2], t_ayar[YOL_AZAMI * 2];
    char t_py[YOL_AZAMI * 2], t_renk[YOL_AZAMI * 2], t_piper[YOL_AZAMI * 2];
    uh_tirnak(wav, t_wav, sizeof t_wav);
    uh_tirnak(g_ses, t_ses, sizeof t_ses);

    char komut[YOL_AZAMI * 16];
    int w;
    if (g_yol == YOL_ONNX) {
        uh_tirnak(g_py, t_py, sizeof t_py);
        uh_tirnak(g_ayar, t_ayar, sizeof t_ayar);
        t_renk[0] = 0;
        if (g_renk[0]) uh_tirnak(g_renk, t_renk, sizeof t_renk);
        w = snprintf(komut, sizeof komut, "%s -I %s %s %s - %s %s %s %s",
                     python_adi() ? python_adi() : "python3",
                     t_py, t_ses, t_ayar, t_wav, g_uzun, t_renk, uh_sessiz());
    } else {
        uh_tirnak(g_piper, t_piper, sizeof t_piper);
        w = snprintf(komut, sizeof komut, "%s --model %s --output_file %s %s",
                     t_piper, t_ses, t_wav, uh_sessiz());
    }
    /* KIRPILMIS KOMUT CALISTIRILMAZ: sessizce kesilen bir komut baska
     * bir dosyaya yazabilir ya da yarim kalir. */
    if (w < 0 || (size_t)w >= sizeof komut) return -1;

    /* Metin BORUDAN gider, komut satırından değil: tırnak, kesme
     * işareti ve kabuk için anlamlı karakterler sorun çıkarmaz.
     * ONNX yolunda da öyle: soylet.py metni "-" ile stdin'den okur. */
    FILE *f = uh_boru_ac(komut);
    if (!f) return -1;
    fputs(metin, f);
    fputc('\n', f);
    int d = uh_boru_kapat(f);
    if (d != 0 || !dosya_var(wav)) { remove(wav); return -1; }

    if (strcmp(g_oynat, G_YERLESIK) == 0) {
        d = uh_yerlesik_oynat(wav);
    } else {
        char oynat[YOL_AZAMI * 4];
        w = snprintf(oynat, sizeof oynat, "%s %s %s", g_oynat, t_wav, uh_sessiz());
        if (w < 0 || (size_t)w >= sizeof oynat) { remove(wav); return -1; }
        d = uh_komut(oynat);
    }
    remove(wav);
    return (d == 0) ? 0 : -1;
}


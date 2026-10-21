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
 *   UMAY_PIPER_RENK   ses rengi düzeltmesi (arac/ses_rengi.py çıktısı)
 *   UMAY_SES_OYNAT    çalıcı komutu (varsayılan: bulunan ilki)
 * ===================================================================== */

/* popen/pclose POSIX'tir; -std=c99 ile gizlenir. */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "umay_piper.h"

#define YOL_AZAMI 512

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

/* Çalıştırılabilir PATH içinde var mı? */
static int komut_var(const char *ad)
{
    const char *yol = getenv("PATH");
    if (!yol) return 0;
    char tampon[YOL_AZAMI];
    const char *p = yol;
    while (*p) {
        const char *iki = strchr(p, ':');
        size_t n = iki ? (size_t)(iki - p) : strlen(p);
        if (n > 0 && n + strlen(ad) + 2 < sizeof tampon) {
            memcpy(tampon, p, n);
            tampon[n] = '/';
            strcpy(tampon + n + 1, ad);
            if (access(tampon, X_OK) == 0) return 1;
        }
        if (!iki) break;
        p = iki + 1;
    }
    return 0;
}

static void varsayilan_ses_yolu(char *c, size_t n)
{
    const char *ev = getenv("HOME");
    if (!ev) ev = ".";
    snprintf(c, n, "%s/.local/share/umay/ses/tr_TR-fahrettin-medium.onnx", ev);
}

/* soylet.py aranir: ayar, kurulum oneki, kaynak agaci. */
static int soylet_bul(char *c, size_t n)
{
    const char *v = getenv("UMAY_PIPER_PY");
    if (v && *v) { snprintf(c, n, "%s", v); return dosya_var(c); }
    const char *ev = getenv("HOME");
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
static int ayar_bul(const char *model, char *c, size_t n)
{
    const char *v = getenv("UMAY_PIPER_AYAR");
    if (v && *v) { snprintf(c, n, "%s", v); return dosya_var(c); }
    snprintf(c, n, "%s.json", model);
    if (dosya_var(c)) return 1;
    size_t L = strlen(model);
    if (L > 5 && strcmp(model + L - 5, ".onnx") == 0) {
        snprintf(c, n, "%.*s.json", (int)(L - 5), model);
        if (dosya_var(c)) return 1;
    }
    return 0;
}

/* python3 + onnxruntime GERCEKTEN var mi? Yalniz dosya varligina
 * bakmak yetmez: onnxruntime kurulu degilse betik her cagrida
 * patlar ve surucu "hazir" gorunur. Sahte basari yok. */
static int onnx_kosulabilir(void)
{
    if (!komut_var("python3") || !komut_var("espeak-ng")) return 0;
    int d = system("python3 -c 'import onnxruntime' >/dev/null 2>&1");
    return d == 0;
}

static void ara(void)
{
    if (g_bakildi) return;
    g_bakildi = 1;
    g_hazir = 0;
    g_yol = YOL_YOK;

    const char *u = getenv("UMAY_PIPER_UZUN");
    if (u && *u) snprintf(g_uzun, sizeof g_uzun, "%s", u);

    /* SES RENGI: olculen perde kusagi ve tayf dengesi duzeltmesi
     * (arac/ses_rengi.py uretir). Yoksa ses ham haliyle kullanilir —
     * eksikligi sessizce bir hataya donmez. */
    const char *r = getenv("UMAY_PIPER_RENK");
    if (r && *r) { if (dosya_var(r)) snprintf(g_renk, sizeof g_renk, "%s", r); }
    else {
#ifdef UMAY_KUT_ONEK
        snprintf(g_renk, sizeof g_renk, "%s/ses/dede_renk.json", UMAY_KUT_ONEK);
        if (!dosya_var(g_renk)) g_renk[0] = 0;
#endif
        if (!g_renk[0]) {
            const char *ev2 = getenv("HOME");
            if (ev2) {
                snprintf(g_renk, sizeof g_renk,
                         "%s/.local/share/umay/ses/dede_renk.json", ev2);
                if (!dosya_var(g_renk)) g_renk[0] = 0;
            }
        }
        if (!g_renk[0]) {
            snprintf(g_renk, sizeof g_renk, "ses/dede_renk.json");
            if (!dosya_var(g_renk)) g_renk[0] = 0;
        }
    }

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
                                         "play", "ffplay", 0 };
        int i;
        for (i = 0; adaylar[i]; i++)
            if (komut_var(adaylar[i])) break;
        if (!adaylar[i]) return;
        if (strcmp(adaylar[i], "ffplay") == 0)
            snprintf(g_oynat, sizeof g_oynat, "ffplay -autoexit -nodisp -loglevel quiet");
        else
            snprintf(g_oynat, sizeof g_oynat, "%s", adaylar[i]);
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

    char wav[YOL_AZAMI];
    snprintf(wav, sizeof wav, "/tmp/umay-dede-%d.wav", (int)getpid());

    char komut[YOL_AZAMI * 6];
    if (g_yol == YOL_ONNX)
        snprintf(komut, sizeof komut,
                 "python3 -I '%s' '%s' '%s' - '%s' %s %s%s%s >/dev/null 2>&1",
                 g_py, g_ses, g_ayar, wav, g_uzun,
                 g_renk[0] ? "'" : "", g_renk[0] ? g_renk : "",
                 g_renk[0] ? "'" : "");
    else
        snprintf(komut, sizeof komut,
                 "%s --model '%s' --output_file '%s' >/dev/null 2>&1",
                 g_piper, g_ses, wav);

    /* Metin BORUDAN gider, komut satırından değil: tırnak, kesme
     * işareti ve kabuk için anlamlı karakterler sorun çıkarmaz.
     * ONNX yolunda da öyle: soylet.py metni "-" ile stdin'den okur. */
    FILE *f = popen(komut, "w");
    if (!f) return -1;
    fputs(metin, f);
    fputc('\n', f);
    int d = pclose(f);
    if (d != 0 || !dosya_var(wav)) { unlink(wav); return -1; }

    snprintf(komut, sizeof komut, "%s '%s' >/dev/null 2>&1", g_oynat, wav);
    d = system(komut);
    unlink(wav);
    return (d == 0) ? 0 : -1;
}


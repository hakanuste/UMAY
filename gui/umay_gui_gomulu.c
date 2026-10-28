/* UMAY — GOMULU on yuz.
 *
 * gui/umay_gui_konsol.c yaniti uçbirime basar; ÜLGEN (gui/umay_ulgen.c) pencereye
 * cizer. Bu ucuncu on yuz yaniti BIR TAMPONA toplar ve uygulamaya teslim eder.
 * Cekirdek (core/) hangi on yuzun bagli oldugunu bilmez: ayni gui_* arayuzu. */
#include <string.h>
#include "umay.h"
#include "umay_gui.h"
#include "umay_gomulu.h"
#include "umay_ogren.h"
#include "umay_host.h"
#include "umay_port.h"
#include "umay_str.h"
#include "umay_kabuk.h"
#include "umay_us.h"
#include "umay_kabul.h"
#include "umay_ortak.h"

static int  g_acik;
static int  g_topla;               /* yalniz sor() sirasinda 1 */
static char g_cevap[GOREV_SONUC_MAX * 4];
static uint32_t g_cevap_n;

void gui_sade_kip(void)    { }
void gui_sade_coz(void)    { }
int  gui_sade_mi(void)     { return 1; }
void gui_girdi_kapat(void) { }

int  gui_ac(int en, int boy) { (void)en; (void)boy; g_acik = 1; return UM_OLDU; }
int  gui_acik_mi(void)       { return g_acik; }
/* Girdiyi uygulama verir (umay_gomulu_sor); stdin'e hic dokunulmaz ve
 * klavye kanali eklenmez. */
int  gui_konsolu_sahiplenir(void) { return 1; }
void gui_kapat(void)         { g_acik = 0; }
int  gui_kapat_istendi(void) { return 0; }
void gui_durum(const char *metin) { (void)metin; }
void gui_ciz(const otag_t *o)     { (void)o; }
void gui_otag_yaz(const otag_t *o){ (void)o; }
int  gui_giris_yokla(umay_ileti_t *i) { (void)i; return 0; }

void gui_satir_ekle(const char *kim, const char *metin)
{
    if (!g_topla || !kim || !metin || !um_esit(kim, "UMAY")) return;
    uint32_t k = (uint32_t)strlen(metin);
    if (g_cevap_n && g_cevap_n + 1 < sizeof g_cevap) g_cevap[g_cevap_n++] = '\n';
    if (g_cevap_n + k + 1 > sizeof g_cevap) k = (uint32_t)sizeof g_cevap - g_cevap_n - 1;
    memcpy(g_cevap + g_cevap_n, metin, k);
    g_cevap_n += k;
    g_cevap[g_cevap_n] = 0;
}

static int g_hazir;

int umay_gomulu_baslat(const char *veri_dizini)
{
    if (g_hazir) return 0;
    if (veri_dizini && *veri_dizini) uh_ortam_ata("UMAY_KUT", veri_dizini);
    if (umay_kur() != 0) return -1;
    g_hazir = 1;
    return 0;
}

int umay_gomulu_sor(const char *soru, char *cevap, uint32_t n)
{
    if (!g_hazir || !soru || !*soru || !cevap || n == 0) return -1;
    g_cevap_n = 0;
    g_cevap[0] = 0;
    g_topla = 1;
    umay_sor(soru);
    /* Ajanlar kosut calisir; yanit gelene dek dondurulur. Ust sinir var. */
    for (int t = 0; t < 600 && umay_mesgul(); t++) {
        umay_tur();
        up_uyu(16);
    }
    umay_tur();
    g_topla = 0;
    uint32_t k = g_cevap_n < n - 1 ? g_cevap_n : n - 1;
    memcpy(cevap, g_cevap, k);
    cevap[k] = 0;
    return (int)k;
}

const char *umay_gomulu_motor(void) { return g_hazir ? us_motor_adi() : "yok"; }

int umay_gomulu_kabul(char *rapor, uint32_t n)
{
    if (!g_hazir || !rapor || n == 0) return -1;
    rapor[0] = 0;
    kabul_sonuc_t s;
    return kabul_calistir(ayar_metin("kutuphane", "kut"), &s, rapor, n);
}

void umay_gomulu_kapat(void)
{
    if (!g_hazir) return;
    umay_kapan();
    g_hazir = 0;
}

const char *umay_gomulu_surum(void) { return UMAY_SURUM; }

/* ---- web arastirmasi ---- */
static umay_ag_getirici_t g_uyg_getirici;
static int64_t uyg_getir(const char *url, char *cik, uint32_t n)
{
    return g_uyg_getirici ? g_uyg_getirici(url, cik, n) : -1;
}

void umay_gomulu_ag_ata(umay_ag_getirici_t getirici)
{
    g_uyg_getirici = getirici;
    ogren_getirici_ata(getirici ? uyg_getir : 0);
}

int umay_gomulu_ogren(const char *konu)
{
    if (!g_hazir) return -1;
    if (konu && *konu) return ogren_arastir("ortak", konu);
    return ogren_gecis(5);
}

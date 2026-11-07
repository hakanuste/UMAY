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
#include "umay_anim.h"
#include "umay_sabah.h"
#include "umay_kasa.h"
#include "umay_gizli.h"

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
    ayar_yaz("anim_otomatik", "0");   /* uygulama kendi zamanlayıcısıyla umay_gomulu_anim_yokla çağırır */
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

/* ---- anımsatıcı / ilaç alarmı ----
 * Uygulama bunu ~20 saniyede bir çağırır; dönen her satırı kendi bildirim yoluyla gösterir
 * (iOS UNUserNotification, Android NotificationManager, masaüstü bildirimi). Satırlar '\n' ile ayrılır.
 * Dönen: vakti gelen satır sayısı (0 = yok), hata ise negatif. "İçtim" yanıtı sohbetten (umay_gomulu_sor) gelir. */
int umay_gomulu_anim_yokla(char *cikti, uint32_t n)
{
    if (!g_hazir || !cikti || n == 0) return -1;
    cikti[0] = 0;
    int64_t simdi = anim_simdi();
    if (simdi < 0) return -1;
    static char satir[6][ANIM_SATIR];
    int k = anim_yokla(simdi, satir, 6);
    for (int i = 0; i < k; i++) {
        if (i) um_ekle(cikti, n, "\x1e");
        if (um_esit(satir[i], ANIM_SABAH_IMI)) {
            static char b[7600];
            sabah_bildirisi(b, sizeof b);
            um_ekle(cikti, n, b);
        } else um_ekle(cikti, n, satir[i]);
    }
    return k;
}

/* ---- gizlilik duvari: uygulama katmani icin ----
 *
 * NEDEN KOPRU GEREKTI: duvar C'de, ag kapisinda duruyordu
 * (ogren_ag_getir ve getir, ikisi de gizli_adres_mi'ye bakiyor).
 * Ama uygulamanin BULUT SESI yolu C'ye HIC ugramiyor: Swift metni
 * dogrudan ElevenLabs'e POST ediyordu. Her yanit sesli okundugu icin,
 * bulut sesi acikken HER YANITIN METNI aygittan cikiyordu — duvarin
 * varlik sebebini bosa cikaran bir acik.
 *
 * Ikinci ve daha genel yanlis: duvar yalniz ADRESE bakiyordu
 * (gizli_adres_mi). C yolu yalniz GET oldugu icin orada tutarliydi,
 * ama bir POST GOVDESI hic incelenmiyordu. Bu kopru metnin kendisini
 * yoklar; gonderilecek govde ne olursa olsun once buradan gecer.
 *
 * Duvar listesi hazir degilse (g_hazir == 0) 1 DONER: emin olunmayan
 * durumda disariya gondermemek dogru taraftir. */
int     umay_gomulu_gizli_mi(const char *metin)
{
    if (!metin || !*metin) return 0;
    if (!g_hazir) return 1;          /* emin degilsen GONDERME */
    return gizli_mi(metin);
}

/* ---- kasa: ince sarmalayicilar (cekirdek umay_kasa.c'dedir) ---- */
int     umay_gomulu_kasa_var(void)                       { return g_hazir ? kasa_var_mi() : 0; }
int     umay_gomulu_kasa_acik(void)                      { return g_hazir ? kasa_acik_mi() : 0; }
int     umay_gomulu_kasa_kur(const char *p)              { return g_hazir ? kasa_kur(p) : -6; }
int     umay_gomulu_kasa_ac(const char *p)               { return g_hazir ? kasa_ac(p) : -6; }
void    umay_gomulu_kasa_kilitle(void)                   { kasa_kilitle(); }
int     umay_gomulu_kasa_say(void)                       { return kasa_say(); }
int     umay_gomulu_kasa_bilgi(int i, char *ad, uint32_t n, int *tur, uint32_t *boy) { return kasa_bilgi(i, ad, n, tur, boy); }
int     umay_gomulu_kasa_yaz(const char *ad, int tur, const void *v, uint32_t n)      { return kasa_yaz(ad, tur, v, n); }
int32_t umay_gomulu_kasa_boy(const char *ad)             { return kasa_boy(ad); }
int32_t umay_gomulu_kasa_oku(const char *ad, void *c, uint32_t n) { return kasa_oku(ad, c, n); }
int     umay_gomulu_kasa_sil(const char *ad)             { return kasa_sil(ad); }
int     umay_gomulu_kasa_parola_degistir(const char *e, const char *y) { return kasa_parola_degistir(e, y); }
int     umay_gomulu_kasa_anahtar(uint8_t c[32])          { return kasa_anahtari_ver(c); }
int     umay_gomulu_kasa_ac_anahtarla(const uint8_t a[32]) { return kasa_ac_anahtarla(a); }

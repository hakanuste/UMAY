/* =====================================================================
 * SES — UMAY'ın ses katmanı yöneticisi.
 *
 * Ses UMAY'a GÖMÜLÜ DEĞİLDİR: değiştirilebilir bir sürücü takılır.
 * UMAY yalnızca ses_soyle / ses_dinle çağırır; arkada hangi motorun
 * durduğunu bilmez. Sürücü yoksa her çağrı UM_DESTEKSIZ döner — sessiz
 * başarı üretilmez.
 *
 * Sürücüler:
 *   ses_bos_surucu()   — hiçbir şey yapmaz, her çağrıya DESTEKSIZ
 *   ses_ozan_surucu()  — Dede Korkut: metni kopuz ezgisine çevirir
 *                        (ÖTÜKEN'de syscall 33, ana bilgisayarda günlük)
 * ===================================================================== */
#include "umay_ses.h"
#include "umay_port.h"
#include "umay_str.h"

static ses_surucu_t *g_surucu = 0;

int ses_surucu_tak(ses_surucu_t *s)
{
    if (g_surucu && g_surucu->kapat) g_surucu->kapat(g_surucu);
    g_surucu = s;
    if (!s) return UM_DESTEKSIZ;
    if (s->ac && s->ac(s, 48000, 2) != UM_OLDU) {
        g_surucu = 0;                 /* açılamayan sürücü takılı SAYILMAZ */
        return UM_YANLIS;
    }
    return UM_OLDU;
}

int ses_var_mi(void) { return g_surucu != 0; }

/* Takili surucunun adi. Gunluge elle yazilmis ad yerine bunu kullan:
 * surucu degisince kayit yaniltici olmasin. */
const char *ses_surucu_adi(void) { return g_surucu ? g_surucu->ad : "yok"; }

int ses_dinle(char *metin, uint32_t n)
{
    if (!g_surucu || !g_surucu->dinle) return UM_DESTEKSIZ;
    return g_surucu->dinle(g_surucu, metin, n);
}

int ses_soyle(const char *metin)
{
    if (!g_surucu || !g_surucu->soyle) return UM_DESTEKSIZ;
    return g_surucu->soyle(g_surucu, metin);
}

/* --- Boş sürücü --- */
static int bos_ac(ses_surucu_t *s, uint32_t hz, uint32_t kanal)
{ (void)s; (void)hz; (void)kanal; return UM_DESTEKSIZ; }

static ses_surucu_t g_bos = { .ad = "bos", .ac = bos_ac };

ses_surucu_t *ses_bos_surucu(void) { return &g_bos; }

/* --- Dede Korkut sürücüsü ---
 * KONUŞUR ya da SUSAR. Üçüncü yol yok.
 *
 * KOPUZ EZGİSİ KALDIRILDI. Eskiden formant sentezi olmazsa metin
 * hecelerine bölünüp kopuz düzeninde bir ezgiye çevriliyordu. Üç
 * nedenle kaldırıldı:
 *   1. Ezgi KONUŞMA DEĞİLDİR. Sözcük taşımaz; "konuştu" sayılması
 *      sahte başarıdır ve ÖTÜKEN'de artık formant sentezi VAR.
 *   2. Yedek yol, asıl yolun bozuk olduğunu GİZLİYORDU: ses_init()
 *      hiç çağrılmadığı için dede_konus() -1 dönüyordu, kopuza
 *      düşülüyordu ve kimse asıl arızayı görmüyordu.
 *   3. Çağırdığı ses_ton() yolu 21 ms'den uzun her notada çekirdek
 *      belleğini taşırıyordu (ozan_ses.c 160-420 ms kullanır).
 * STT (dinle) yoktur. */
static int ozan_ac(ses_surucu_t *s, uint32_t hz, uint32_t kanal)
{
    (void)s; (void)hz; (void)kanal;
    /* Ses yolu yoksa sürücü AÇILMAZ. Açılmış gibi davranıp metni
     * günlüğe basmak, konuşuyormuş izlenimi verirdi. */
    return up_ses_var_mi() ? UM_OLDU : UM_DESTEKSIZ;
}

static int ozan_soyle(ses_surucu_t *s, const char *metin)
{
    (void)s;
    if (!metin || !*metin) return UM_YANLIS;
    /* GERÇEK konuşma: Türkçe formant sentezi. Olmazsa DESTEKSIZ döner
     * ve UMAY yalnız yazıyla yanıtladığını BİLİR.
     *
     * SAHTE BAŞARI DÜZELTİLDİ: burada iki yol da başarısız olsa yine
     * UM_OLDU dönüyordu. ÖTÜKEN'de ses_init() hiç çağrılmadığı için
     * ses_var() sıfırdı ve dede_konus() ilk satırında -1 ile çıkıyordu
     * — ama UMAY "konuştum" diyordu. Dosyanın başındaki "sessiz başarı
     * üretilmez" ilkesi tam burada çiğneniyordu. */
    return (up_dede_konus(metin) == 0) ? UM_OLDU : UM_DESTEKSIZ;
}

static ses_surucu_t g_ozan = {
    .ad = "dede-korkut",
    .ac = ozan_ac,
    .soyle = ozan_soyle
};

ses_surucu_t *ses_ozan_surucu(void) { return &g_ozan; }

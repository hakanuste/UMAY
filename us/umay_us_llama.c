/* =====================================================================
 * UMAY — GÖMÜLÜ LLM arka ucu (llama.cpp, motor/ altında).
 *
 * NE YAPAR: US katmanının çıkarımını UMAY'ın İÇİNDEKİ dil modeline verir;
 * Ollama ya da başka bir sunucu gerekmez, ağ gerekmez, veri dışarı çıkmaz.
 *
 * MODEL: GGUF dosyası. Sırayla aranır:
 *   1. UMAY_LLM ortam değişkeni (dosya yolu)
 *   2. ayar "llm" (dosya yolu)
 *   3. hazirla()'ya verilen yol, ".gguf" ile bitiyorsa
 *   4. <kütüphane>/model/ içindeki ilk .gguf
 * Kurulum: ./kur.sh --model   (bkz. docs/MOTOR.md)
 *
 * DÜRÜSTLÜK:
 *   - Motor bu yapıda yoksa ya da model bulunamazsa hazirla() BAŞARISIZ döner
 *     (sahte başarı yok); US sıradaki arka uca / stub'a düşer.
 *   - Bağlam taşarsa dayanak yarıya kısaltılıp bir kez daha denenir; yine
 *     taşarsa BAŞARISIZ döner (kesilmiş dayanakla "sağlam" konuşmaz).
 *   - Model dayanak dışına çıkmasın diye sistem istemi dayanağa bağlar.
 * ===================================================================== */
#include "umay_us.h"
#include "umay_motor.h"
#include "umay_ortak.h"
#include "umay_port.h"
#include "umay_str.h"

#define LLM_ISTEM_MAX 12288

static int g_hazir;

/* Çıktı sağlam mı? Geçerli UTF-8 olmalı ve okunur karakter oranı yüksek olmalı.
 * Bozuk/anlamsız bayt yığını yanıt diye sunulmaz (üst katman stub'a düşer). */
int us_llama_cikti_saglam(const char *s) {
    const uint8_t *p = (const uint8_t *)s;
    uint32_t toplam = 0, okunur = 0;
    while (*p) {
        uint32_t k = 1;
        if (*p < 0x80) { k = 1; }
        else if ((*p & 0xE0) == 0xC0) k = 2;
        else if ((*p & 0xF0) == 0xE0) k = 3;
        else if ((*p & 0xF8) == 0xF0) k = 4;
        else return 0;
        for (uint32_t i = 1; i < k; i++) if ((p[i] & 0xC0) != 0x80) return 0;
        if (k == 3 && p[0] == 0xE0 && p[1] < 0xA0) return 0;           /* aşırı uzun kodlama */
        if (k == 1) {
            uint8_t c = *p;
            if (c == '\n' || c == '\t' || (c >= 0x20 && c < 0x7F)) okunur++;
        } else if (k == 2 && p[0] >= 0xC3 && p[0] <= 0xC5) okunur++;    /* Latin-1 Suppl. / Latin Ext-A: Türkçe harfler */
        else if (k >= 2) okunur += 0;                                  /* başka yazılar sayılmaz (Türkçe çıktı beklenir) */
        toplam++;
        p += k;
    }
    return toplam > 0 && okunur * 100 >= toplam * 85;
}

static int gguf_mi(const char *y) {
    uint32_t u = y ? um_uzun(y) : 0;
    return u > 5 && um_esit(y + u - 5, ".gguf");
}

/* Model dosyasını bulur; yol'a yazar. 1 = bulundu. */
static int model_bul(const char *verilen, char *yol, uint32_t n) {
    const char *e = up_ortam("UMAY_LLM");
    if (e && *e) { um_kopya(yol, n, e); return 1; }
    const char *a = ayar_metin("llm", "");
    if (a && *a) { um_kopya(yol, n, a); return 1; }
    if (gguf_mi(verilen)) { um_kopya(yol, n, verilen); return 1; }
    char dizin[UMAY_YOL_MAX];
    um_bicim(dizin, sizeof dizin, "%s/model", ayar_metin("kutuphane", "kut"));
    up_girdi_t g[32];
    int adet = up_dizin_listele(dizin, g, 0, 32);
    for (int i = 0; i < adet; i++) {
        if (!g[i].dizin_mi && gguf_mi(g[i].ad)) { um_bicim(yol, n, "%s/%s", dizin, g[i].ad); return 1; }
    }
    return 0;
}

static int llama_hazirla(umay_us_t *u, const char *model_yolu) {
    (void)u;
    const umay_motor_api_t *m = umay_motor_api();
    if (!m) { gun_yaz("US-LLAMA: bu yapıda gömülü motor yok (arac/motor.sh ile motorlu yap)."); return UM_YOK; }
    char yol[UMAY_YOL_MAX];
    if (!model_bul(model_yolu, yol, sizeof yol)) {
        gun_yaz("US-LLAMA: model yok (./kur.sh --model ya da UMAY_LLM=<dosya.gguf>).");
        return UM_YOK;
    }
    if (up_boy(yol) <= 0) { gun_yaz("US-LLAMA: model dosyası okunamadı: %s", yol); return UM_YOK; }
    int bag = (int)ayar_sayi("llm_baglam", 2048);
    int is = (int)ayar_sayi("llm_is", 0);
    if (m->yukle(yol, bag, is) != 0) { gun_yaz("US-LLAMA: model yüklenemedi: %s", yol); return UM_YOK; }
    g_hazir = 1;
    gun_yaz("US-LLAMA: hazır — %s", m->ad ? m->ad() : "llama");
    return UM_OLDU;
}

static const char *SISTEM_KURAL =
    "\n\nKURALLAR: Yalnız 'DAYANAK' bölümündeki bilgiye dayanarak yanıtla. Dayanakta olmayanı uydurma; "
    "dayanak yetmiyorsa 'bunu dayanağımda bulamadım' de. Öz Türkçe, kısa ve açık yaz. "
    "Dayanakta sayı ya da ölçü varsa aynen koru.";

static void dusunceyi_at(char *s) {
    /* <think>...</think> bloğunu at (Qwen3 düşünme kipi kapalı olsa da güvence) */
    char *a = s;
    for (;;) {
        char *b = (char *)um_icinde(a, "<think>");
        if (!b) break;
        char *e = (char *)um_icinde(b, "</think>");
        if (!e) { *b = 0; break; }
        e += 8;
        uint32_t k = um_uzun(e);
        um_bellek_kopya(b, e, k + 1);
    }
    char *p = s;
    while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++;
    if (p != s) um_bellek_kopya(s, p, um_uzun(p) + 1);
    uint32_t u = um_uzun(s);
    while (u && (s[u - 1] == ' ' || s[u - 1] == '\n' || s[u - 1] == '\r')) s[--u] = 0;
}

static int llama_uret(umay_us_t *u, const us_istem_t *i, char *cikti, uint32_t n) {
    (void)u;
    const umay_motor_api_t *m = umay_motor_api();
    if (!m || !g_hazir || !i || !cikti || n == 0) return UM_YOK;
    char *sistem = (char *)up_ayir(LLM_ISTEM_MAX), *kul = (char *)up_ayir(LLM_ISTEM_MAX);
    if (!sistem || !kul) { if (sistem) up_biraktir(sistem); if (kul) up_biraktir(kul); return UM_YOK; }
    int sonuc = UM_YOK;
    uint32_t im = i->en_cok_im ? i->en_cok_im : 256;
    if (im > 512) im = 512;
    const char *baglam = i->baglam ? i->baglam : "(yok)";
    uint32_t bl = um_uzun(baglam);
    for (int deneme = 0; deneme < 2; deneme++) {
        um_bicim(sistem, LLM_ISTEM_MAX, "%s%s", i->kisilik ? i->kisilik : "", SISTEM_KURAL);
        char *bk = (char *)up_ayir((uint64_t)bl + 1);
        if (!bk) break;
        um_bellek_kopya(bk, baglam, bl); bk[bl] = 0;
        um_bicim(kul, LLM_ISTEM_MAX, "DAYANAK:\n%s\n\nSORU: %s\n/no_think", bk, i->soru ? i->soru : "");
        up_biraktir(bk);
        int r = m->uret(sistem, kul, im, cikti, n);
        if (r == 0) {
            dusunceyi_at(cikti);
            if (cikti[0] && !us_llama_cikti_saglam(cikti)) {
                gun_yaz("US-LLAMA: çıktı okunur metin değil (bozuk ya da anlamsız), yanıt saymıyorum.");
                cikti[0] = 0;
            }
            sonuc = cikti[0] ? UM_OLDU : UM_YOK;
            break;
        }
        if (r != UMAY_MOTOR_TASTI) { gun_yaz("US-LLAMA: üretim başarısız (%d).", r); break; }
        /* bağlam taştı: dayanağı yarıya kısalt (cümle başında, UTF-8 sınırında) ve bir kez daha dene */
        gun_yaz("US-LLAMA: bağlam taştı, dayanağı kısaltıyorum.");
        bl /= 2;
        while (bl > 0 && ((uint8_t)baglam[bl] & 0xC0) == 0x80) bl--;
        if (bl < 200) break;
    }
    up_biraktir(sistem); up_biraktir(kul);
    return sonuc;
}

static void llama_kapat(umay_us_t *u) {
    (void)u;
    const umay_motor_api_t *m = umay_motor_api();
    if (m && g_hazir) m->kapat();
    g_hazir = 0;
}

static umay_us_t g_llama = {
    .ad = "llama", .yetenek = 0,
    .hazirla = llama_hazirla, .uret = llama_uret, .kapat = llama_kapat, .ic = 0
};

umay_us_t *us_llama_motor(void) { return &g_llama; }

/* ONARICI — aksaklığı bulur, ONARIM ÖNERİSİ üretir.
 * Kendiliğinden hiçbir şeyi yazmaz/silmez: her yıkıcı iş onay_iste()
 * ile kullanıcıya sorulur (V9.2 onay_bekleyenler mantığı). */
#include "umay_alp.h"
#include "umay_port.h"
#include "umay_str.h"
#include "umay_onay.h"
#include "umay_ortak.h"

typedef struct {
    int  adim_no;
    int  bulgu;
    char rapor[UMAY_ILETI_MAX];
    char is[UMAY_YOL_MAX];
} onarici_ic_t;

static onarici_ic_t g_ic;

/* Onarılabilir bilinen aksaklıklar: eksik dizin/dosya
 *
 * YOLLAR PORT'TAN SORULUR, GÖMÜLÜ DEĞİL.
 *
 * Eskiden burada "/kut/umay", "/kut/umay/ayar.txt" ve
 * "/kut/umay/bengi.txt" yazılıydı — bunlar ÖTÜKEN'in düz FAT16
 * kökündeki yollar. Ana bilgisayarda kütüphane başka yerdedir
 * (UMAY_KUT, çalışma dizinindeki kut/, ~/.local/share/umay/kut …).
 *
 * ÖLÇÜLDÜ: sağlam bir kurulumda ONARICI üç şeyi "eksik" bildiriyor,
 * GÖZCÜ de aynı listeden "2 gerekli iz eksik" diyordu. Yani dizge
 * yerindeyken kendini BOZUK sanıyor — ve üstüne dosya sisteminin
 * KÖKÜNDE "/kut/umay" dizini yaratmayı öneriyordu.
 *
 * Yanlış yeri arayan bir denetim, hiç denetlememekten kötüdür:
 * gerçek bir aksaklık çıktığında kullanıcı uyarıyı zaten görmezden
 * gelmeyi öğrenmiş olur. Artık kök up_kut_kok()'tan, dosya yolları
 * up_kut_dosya()'dan gelir; iki yapıda da doğru yere bakılır. */
#define DENETIM_SAYI 3

static const struct { const char *ad; const char *ne; const char *cozum; } DENETIM[] = {
    { 0,           "kütüphane dizini",  "dizini yarat" },
    { "ayar.txt",  "ayar bitiği",       "varsayılan ayarı yaz" },
    { "bengi.txt", "kalıcı bellek",     "boş bengi bitiği aç" },
};

/* i. denetimin GERÇEK yolunu yazar. ad == 0 ise kütüphane kökü. */
static void denetim_yolu(int i, char *c, uint32_t n)
{
    if (!DENETIM[i].ad) {
        const char *k = up_kut_kok();
        um_kopya(c, n, (k && *k) ? k : ".");
        return;
    }
    up_kut_dosya(DENETIM[i].ad, c, n);
}

static int uygun(const gorev_t *g) { (void)g; return 1; }

static int basla(alp_t *a, const gorev_t *g) {
    (void)g;
    um_bellek_doldur(&g_ic, 0, sizeof g_ic);
    a->ic = &g_ic;
    return UM_OLDU;
}

static alp_durum_t adim(alp_t *a) {
    onarici_ic_t *ic = (onarici_ic_t*)a->ic;

    if (ic->adim_no == 0) {
        um_kopya(ic->rapor, sizeof ic->rapor, "");
        ic->adim_no++;
        return ALP_KOSUYOR;
    }

    int i = ic->adim_no - 1;
    if (i < DENETIM_SAYI) {
        char yol[UMAY_YOL_MAX];
        denetim_yolu(i, yol, sizeof yol);
        if (!up_var_mi(yol)) {
            char s[256];
            um_bicim(s, sizeof s, "Eksik: %s (%s) → önerim: %s.\n",
                     yol, DENETIM[i].ne, DENETIM[i].cozum);
            um_ekle(ic->rapor, sizeof ic->rapor, s);
            um_kopya(ic->is, sizeof ic->is, yol);
            ic->bulgu++;
        }
        ic->adim_no++;
        return ALP_KOSUYOR;
    }

    /* Bulgu varsa onay iste — yazma işini KENDİ BAŞINA yapmaz */
    if (ic->bulgu > 0) {
        char ozet[256];
        um_bicim(ozet, sizeof ozet, "%d eksik iz bulundu. Onarım için yazma yetkisi ve onayın gerek.", ic->bulgu);
        const char *kimlik = onay_iste("onarim", "Eksik izlerin onarımı", ozet, ic->is);
        char s[192];
        if (kimlik) {
            um_bicim(s, sizeof s, "Elimi işe atmadan sana sorarım. Onay belgesi: %s\n"
                                  "\"onay ver %s\" dersen düzeltirim.\n", kimlik, kimlik);
        } else {
            um_kopya(s, sizeof s, "Onay yeri dolu; önce eski onayları karara bağla.\n");
        }
        um_ekle(ic->rapor, sizeof ic->rapor, s);
    } else {
        um_ekle(ic->rapor, sizeof ic->rapor,
                "Aksaklık aradım, bulamadım. Dizge yerli yerinde duruyor.\n");
    }
    return ALP_BITTI;
}

static void topla(alp_t *a, sonuc_t *s) {
    onarici_ic_t *ic = (onarici_ic_t*)a->ic;
    um_kopya(s->oz, sizeof s->oz, ic->rapor);
}

static alp_t g_onarici = {
    .ad = "ONARICI", .is = "aksaklık bulma ve onarım önerisi",
    .alan = ALAN_YOK,
    .tur_maskesi = GT_BIT(GT_ONAR),
    .kategori = { 0 },
    .yetki_gerek = YET_DOSYA_OKU,
    .uygun_mu = uygun, .basla = basla, .adim = adim, .topla = topla,
    .durum = ALP_BOS
};
alp_t *alp_onarici(void) { return &g_onarici; }

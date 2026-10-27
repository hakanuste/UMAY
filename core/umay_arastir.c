#include "umay_arastir.h"
#include "umay_bekci.h"
#include "umay_ogren.h"
#include "umay_ortak.h"
#include "umay_port.h"
#include "umay_str.h"

/* terimin kökü: ilk 5 bayt (UTF-8 sınırında) — Türkçe ekler sonda gelir */
static void kok(char *c, uint32_t n, const char *t) {
    um_kopya(c, n, t);
    if (um_uzun(c) > 5) {
        uint32_t k = 5;
        while (k > 0 && ((uint8_t)c[k] & 0xC0u) == 0x80u) k--;
        c[k] = 0;
    }
}

static int ayni_kaynak(const baglam_t *a, const baglam_t *b) {
    if (um_esit(a->kaynak, b->kaynak) && um_esit(a->baslik, b->baslik)) return 1;
    /* aynı metin başka dosyada (örn. aynı makale iki kez öğrenildi) */
    return a->alinti[0] && um_esit_n(a->alinti, b->alinti, 120) && um_uzun(a->alinti) >= 120;
}

static int kaynak_ekle(arastirma_t *r, const baglam_t *b) {
    for (int i = 0; i < r->kaynak_sayi; i++) if (ayni_kaynak(&r->kaynak[i], b)) return 0;
    if (r->kaynak_sayi >= ARASTIR_KAYNAK_MAX) return 0;
    r->kaynak[r->kaynak_sayi++] = *b;
    return 1;
}

/* bir kaynak metni terimi içeriyor mu */
static int icerir(const baglam_t *b, const char *terim) {
    char ham[BILGI_ALINTI_MAX + 100], kucuk[BILGI_ALINTI_MAX + 100], k[24];
    um_bicim(ham, sizeof ham, "%s %s", b->baslik, b->alinti);
    um_kucult(kucuk, sizeof kucuk, ham);
    kok(k, sizeof k, terim);
    return k[0] && um_icinde(kucuk, k) != 0;
}

static void kapsam_hesapla(arastirma_t *r) {
    int bulunan = 0;
    for (int t = 0; t < r->terim_sayi; t++) {
        r->terim_kaynak[t] = 0;
        for (int i = 0; i < r->kaynak_sayi; i++)
            if (icerir(&r->kaynak[i], r->terim[t])) r->terim_kaynak[t]++;
        if (r->terim_kaynak[t] > 0) bulunan++;
    }
    r->kapsam = r->terim_sayi ? (bulunan * 100) / r->terim_sayi : 0;
}

static void plan_kur(arastirma_t *r) {
    char ham[ARASTIR_TERIM_MAX + 4][24], kel[ARASTIR_TERIM_MAX][24];
    int h = bilgi_kelime_ayikla(r->soru, ham, ARASTIR_TERIM_MAX + 4), n = 0;
    /* komut sözcükleri ("araştır", "derle"...) konu değildir; terim sayılmaz */
    for (int i = 0; i < h && n < ARASTIR_TERIM_MAX; i++) {
        int komut = ogren_komut_sozcugu(ham[i]);
        if (!komut) um_kopya(kel[n++], sizeof kel[0], ham[i]);
    }
    r->terim_sayi = n;
    for (int i = 0; i < n; i++) um_kopya(r->terim[i], sizeof r->terim[i], kel[i]);
    if (n == 0) return;
    /* açı 1: hepsi birlikte */
    r->aci[0][0] = 0;
    for (int i = 0; i < n; i++) { if (i) um_ekle(r->aci[0], sizeof r->aci[0], " "); um_ekle(r->aci[0], sizeof r->aci[0], kel[i]); }
    r->aci_sayi = 1;
    if (n < 2) return;
    /* sonraki açılar: en uzun (en ayırt edici) tek sözcükler */
    int kullanildi[ARASTIR_TERIM_MAX] = {0};
    for (int a = 1; a < ARASTIR_ACI_MAX && a <= n && a <= 3; a++) {
        int en = -1; uint32_t en_u = 0;
        for (int i = 0; i < n; i++)
            if (!kullanildi[i] && um_uzun(kel[i]) > en_u) { en = i; en_u = um_uzun(kel[i]); }
        if (en < 0) break;
        kullanildi[en] = 1;
        um_kopya(r->aci[r->aci_sayi++], sizeof r->aci[0], kel[en]);
    }
}

/* tek bir arama: bekçi sorulur, sonuçlar kaynaklara eklenir. 0 = yapıldı, 1 = engellendi */
static int ara(arastirma_t *r, bekci_t *b, const char *aci) {
    bekci_karar_t k = bekci_sor(b, "bilgi", aci);
    if (k.uyari) { r->bekci_uyari++; um_kopya(r->bekci_not, sizeof r->bekci_not, k.neden); }
    if (k.engel) { r->bekci_engel++; um_kopya(r->bekci_not, sizeof r->bekci_not, k.neden); return 1; }
    baglam_t tmp[3];
    int n = bilgi_baglam_bul(aci, tmp, 3);
    r->arama++;
    for (int i = 0; i < n; i++) kaynak_ekle(r, &tmp[i]);
    return 0;
}

static void guven_ver(arastirma_t *r) {
    int web_disi = 0, ayri = r->kaynak_sayi;
    for (int i = 0; i < r->kaynak_sayi; i++) if (r->kaynak[i].kaynak_tur != KT_DIS) web_disi++;
    r->catisma = 0;
    catisma_t c;
    um_bellek_doldur(&c, 0, sizeof c);
    /* ÇATIŞMA yalnız güvenilen kaynaklar arasında aranır; doğrulanmamış web
     * kaydı bir doktrinle "çatışıyor" sayılırsa her araştırma zayıf çıkar. */
    baglam_t guvenilen[ARASTIR_KAYNAK_MAX];
    int gn = 0;
    for (int i = 0; i < r->kaynak_sayi; i++)
        if (r->kaynak[i].kaynak_tur != KT_DIS) guvenilen[gn++] = r->kaynak[i];
    if (gn >= 2 && bilgi_catisma_bul(guvenilen, gn, &c) && c.var) {
        r->catisma = 1;
        um_bicim(r->catisma_notu, sizeof r->catisma_notu,
                 "%s alanında iki ayrı kaynak: %s ile %s", c.kategori, c.kaynak_a, c.kaynak_b);
    }
    if (r->kaynak_sayi == 0) {
        r->guven = GUVEN_YOK;
        um_kopya(r->guven_neden, sizeof r->guven_neden, "hiçbir kaynakta dayanak bulunamadı");
    } else if (r->catisma) {
        r->guven = GUVEN_ZAYIF;
        um_kopya(r->guven_neden, sizeof r->guven_neden, "kaynaklar birbiriyle çelişiyor olabilir");
    } else if (r->kapsam < 50) {
        r->guven = GUVEN_ZAYIF;
        um_bicim(r->guven_neden, sizeof r->guven_neden, "soru yalnız %%%d karşılanıyor", r->kapsam);
    } else if (ayri >= 2 && r->kapsam >= 70 && web_disi >= 1) {
        r->guven = GUVEN_SAGLAM;
        um_bicim(r->guven_neden, sizeof r->guven_neden, "%d ayrı kaynak, kapsam %%%d", ayri, r->kapsam);
    } else {
        r->guven = GUVEN_ORTA;
        if (web_disi == 0)
            um_kopya(r->guven_neden, sizeof r->guven_neden, "yalnız doğrulanmamış web kaynağı var");
        else
            um_bicim(r->guven_neden, sizeof r->guven_neden, "%d kaynak, kapsam %%%d", ayri, r->kapsam);
    }
}

int arastir_yurut(const char *soru, const char *kategori, int web, arastirma_t *r) {
    um_bellek_doldur(r, 0, sizeof *r);
    um_kopya(r->soru, sizeof r->soru, soru ? soru : "");
    plan_kur(r);
    if (r->aci_sayi == 0) { r->guven = GUVEN_YOK; um_kopya(r->guven_neden, sizeof r->guven_neden, "soruda aranacak sözcük yok"); return UM_YANLIS; }

    bekci_t b;
    bekci_kur(&b);
    for (int a = 0; a < r->aci_sayi && r->arama < ARASTIR_BUTCE; a++)
        if (ara(r, &b, r->aci[a])) break;
    kapsam_hesapla(r);

    /* GENİŞLET: kapsam yetmiyor, web açıksa kapat ve yeniden ara */
    if (web && r->kapsam < 100 && ogren_acik() && r->arama < ARASTIR_BUTCE) {
        bekci_karar_t k = bekci_sor(&b, "web", r->aci[0]);
        if (k.engel) { r->bekci_engel++; um_kopya(r->bekci_not, sizeof r->bekci_not, k.neden); }
        else {
            r->arama++;
            int n = ogren_hemen(kategori ? kategori : "ortak", r->soru);
            if (n > 0) {
                r->web_parca = n;
                ara(r, &b, r->aci[0]);               /* öğrenileni tam açıyla yeniden ara */
                kapsam_hesapla(r);
            }
        }
    }
    guven_ver(r);
    return UM_OLDU;
}

int arastir_kapsam(const char *soru) {
    arastirma_t *r = (arastirma_t *)up_ayir(sizeof *r);
    if (!r) return 0;
    arastir_yurut(soru, "ortak", 0, r);
    int k = r->kapsam;
    up_biraktir(r);
    return k;
}

/* alıntıyı kısaltır (UTF-8 sınırında) */
static void ozet(char *c, uint32_t n, const char *alinti) {
    char t[BILGI_ALINTI_MAX + 8];
    uint32_t o = 0;
    int bosluk = 1;
    for (const char *p = alinti; *p && o + 1 < sizeof t; p++) {     /* satırları tek boşluğa indir */
        if (*p == '\n' || *p == '\r' || *p == '\t' || *p == ' ') { if (!bosluk) t[o++] = ' '; bosluk = 1; }
        else { t[o++] = *p; bosluk = 0; }
    }
    t[o] = 0;
    if (um_uzun(t) >= n) {
        uint32_t k = n - 4;
        while (k > 0 && ((uint8_t)t[k] & 0xC0u) == 0x80u) k--;
        t[k] = 0;
        um_ekle(t, sizeof t, "...");
    }
    um_kopya(c, n, t);
}

static const char *tur_adi(kaynak_tur_t t) {
    switch (t) {
    case KT_DOKTRIN: return "doktrin";
    case KT_BELGE:   return "belge";
    case KT_DIS:     return "doğrulanmamış web";
    default:         return "kayıt";
    }
}

uint32_t arastir_rapor(const arastirma_t *r, char *c, uint32_t n) {
    char s[512];
    c[0] = 0;
    um_bicim(s, sizeof s, "ARAŞTIRMA: %s\nPlan: %d açı, %d arama%s.\n",
             r->soru, r->aci_sayi, r->arama,
             r->web_parca ? ", web'den öğrenildi" : "");
    um_ekle(c, n, s);

    if (r->kaynak_sayi == 0) {
        um_ekle(c, n, "Bulgu: yok. Kütüphanede de (açık ise) web'de de buna değen bir kayıt bulamadım; uydurmam.\n");
        um_bicim(s, sizeof s, "Güven: %s — %s\n", us_guven_adi(r->guven), r->guven_neden);
        um_ekle(c, n, s);
        return um_uzun(c);
    }

    um_ekle(c, n, "Bulgular:\n");
    int goster = r->kaynak_sayi < 3 ? r->kaynak_sayi : 3;
    for (int i = 0; i < goster; i++) {
        char oz[300];
        ozet(oz, sizeof oz, r->kaynak[i].alinti);
        um_bicim(s, sizeof s, " [%d] %s\n", i + 1, oz);
        um_ekle(c, n, s);
    }

    /* çapraz denetim: terim başına ayrı kaynak sayısı */
    char dogrulanan[160], tek[160], eksik[160];
    dogrulanan[0] = tek[0] = eksik[0] = 0;
    for (int t = 0; t < r->terim_sayi; t++) {
        char *hedef = r->terim_kaynak[t] >= 2 ? dogrulanan : (r->terim_kaynak[t] == 1 ? tek : eksik);
        uint32_t hn = r->terim_kaynak[t] >= 2 ? sizeof dogrulanan : (r->terim_kaynak[t] == 1 ? sizeof tek : sizeof eksik);
        if (hedef[0]) um_ekle(hedef, hn, ", ");
        um_ekle(hedef, hn, r->terim[t]);
    }
    um_bicim(s, sizeof s, "Kapsam: %%%d\n", r->kapsam);
    um_ekle(c, n, s);
    if (dogrulanan[0]) { um_bicim(s, sizeof s, "Çapraz denetim: birden çok kaynakta geçen: %s\n", dogrulanan); um_ekle(c, n, s); }
    if (tek[0])        { um_bicim(s, sizeof s, "Tek kaynakta geçen: %s\n", tek); um_ekle(c, n, s); }
    if (eksik[0])      { um_bicim(s, sizeof s, "Eksik (hiçbir kaynakta yok): %s\n", eksik); um_ekle(c, n, s); }
    if (r->catisma)    { um_bicim(s, sizeof s, "ÇATIŞMA: %s\n", r->catisma_notu); um_ekle(c, n, s); }
    if (r->bekci_uyari || r->bekci_engel) {
        um_bicim(s, sizeof s, "Bekçi: %s%s\n", r->bekci_engel ? "döngü engellendi — " : "uyarı — ", r->bekci_not);
        um_ekle(c, n, s);
    }
    um_bicim(s, sizeof s, "Güven: %s — %s\n", us_guven_adi(r->guven), r->guven_neden);
    um_ekle(c, n, s);

    um_ekle(c, n, "Kaynaklar:\n");
    for (int i = 0; i < r->kaynak_sayi; i++) {
        const baglam_t *k = &r->kaynak[i];
        um_bicim(s, sizeof s, " [%d] %s — %s (%s%s%s)\n", i + 1, k->baslik, k->kaynak,
                 tur_adi(k->kaynak_tur),
                 um_esit(k->dil, "tr") ? "" : ", dil: ", um_esit(k->dil, "tr") ? "" : k->dil);
        um_ekle(c, n, s);
    }
    return um_uzun(c);
}

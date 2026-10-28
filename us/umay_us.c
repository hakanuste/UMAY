/* =====================================================================
 * US ÇIKARIM KATMANI
 * Motorun üstünde durur. Dayanağı, kaynağını, uzmanları, uyarıları ve
 * güven durumunu kaybetmeden tek çıktı sözleşmesine dönüştürür.
 * ===================================================================== */
#include "umay_us.h"
#include "umay_str.h"
#include "umay_port.h"
#include "umay_ortak.h"

static umay_us_t *g_motor;

const char *us_guven_adi(us_guven_t g) {
    switch (g) {
    case GUVEN_SAGLAM: return "sağlam";
    case GUVEN_ORTA:   return "orta";
    case GUVEN_ZAYIF:  return "zayıf";
    default:           return "yok";
    }
}

static const char *kaynak_tur_adi(kaynak_tur_t k) {
    switch (k) {
    case KT_DOKTRIN: return "doktrin";
    case KT_BELGE:   return "belge";
    case KT_DIS:     return "dış kayıt";
    default:         return "bilinmez";
    }
}

/* ---------------- motor yönetimi ---------------- */

int us_motor_tak(umay_us_t *m) {
    g_motor = m ? m : us_stub_motor();
    return g_motor == us_stub_motor() ? UM_DESTEKSIZ : UM_OLDU;
}

umay_us_t *umay_us_al(void) {
    if (!g_motor) g_motor = us_stub_motor();
    return g_motor;
}

const char *us_motor_adi(void) { return umay_us_al()->ad; }

int us_kur(const char *model_yolu) {
    /* Arka uç sırası: PORT'un önerdiği (ÖTÜKEN'de yerel US1), sonra IPC,
     * en son stub. Her birinin hazırlanması GERÇEKTEN başarılı olmalı;
     * başarısız olan atlanır ve bu günlüğe yazılır. Sahte başarı yok. */
    umay_us_t *adaylar[3];
    int n = 0;
    umay_us_t *p = (umay_us_t *)up_us_motoru();
    if (p) adaylar[n++] = p;
    umay_us_t *ipc = us_ipc_motor();
    if (ipc != p) adaylar[n++] = ipc;

    for (int i = 0; i < n; i++) {
        umay_us_t *m = adaylar[i];
        gun_yaz("US: %s deneniyor.", m->ad);
        if (!m->hazirla || m->hazirla(m, model_yolu) == UM_OLDU) {
            g_motor = m;
            gun_yaz("US: %s etkin.", m->ad);
            return UM_OLDU;
        }
        gun_yaz("US: %s hazırlanamadı.", m->ad);
    }

    g_motor = us_stub_motor();
    if (g_motor->hazirla) g_motor->hazirla(g_motor, 0);
    gun_yaz("US: gerçek motor yok, stub'a düştüm.");
    return UM_DESTEKSIZ;
}

/* ---------------- güven derecesi ---------------- */

static us_guven_t guven_hesapla(const us_derle_t *i) {
    if (i->baglam_sayi <= 0) return GUVEN_YOK;
    if (i->catisma && i->catisma->var) return GUVEN_ZAYIF;

    us_guven_t g = (i->baglam_sayi >= 2) ? GUVEN_SAGLAM : GUVEN_ORTA;
    /* dayanak nereden geldiyse tavanı o belirler */
    if (i->nereden == US_GENEL_DEPO && g > GUVEN_ZAYIF) g = GUVEN_ZAYIF;
    else if (i->nereden == US_ORTAK_HAVUZ && g > GUVEN_ORTA) g = GUVEN_ORTA;
    /* tek kelimelik zayıf eşleşme sağlam sayılmaz */
    if (g == GUVEN_SAGLAM && i->baglam[0].skor <= 1) g = GUVEN_ORTA;
    return g;
}

/* ---------------- çıkarım ---------------- */

static void uyari_ekle(us_cikti_t *o, const char *s) {
    if (!s || !*s || o->uyari_sayi >= US_UYARI_MAX) return;
    for (int i = 0; i < o->uyari_sayi; i++) if (um_esit(o->uyari[i], s)) return;
    um_kopya(o->uyari[o->uyari_sayi++], 160, s);
}

static void uzman_ekle(us_cikti_t *o, const char *ad) {
    if (!ad || !*ad || o->uzman_sayi >= US_UZMAN_MAX) return;
    for (int i = 0; i < o->uzman_sayi; i++) if (um_esit(o->uzman[i], ad)) return;
    um_kopya(o->uzman[o->uzman_sayi++], UMAY_AD_MAX, ad);
}

static void dayanak_ekle(us_cikti_t *o, const baglam_t *b) {
    if (o->dayanak_sayi >= US_DAYANAK_MAX) return;
    us_dayanak_t *d = &o->dayanak[o->dayanak_sayi++];
    um_kopya(d->kategori, sizeof d->kategori, b->kategori);
    um_kopya(d->baslik,   sizeof d->baslik,   b->baslik);
    um_kopya(d->kaynak,   sizeof d->kaynak,   b->kaynak);
    d->kaynak_tur = b->kaynak_tur;
    um_kopya(d->dil, sizeof d->dil, b->dil[0] ? b->dil : "tr");
    um_kopya(d->ozgun_dil, sizeof d->ozgun_dil, b->ozgun_dil);
    d->skor = b->skor;
}

/* Dil kodunu Türkçe adıyla söyler. Bilinmeyen kod olduğu gibi geçer:
 * uydurma ad üretmek, kaynağı yanlış tanıtmak olur. */
static const char *dil_adi(const char *k) {
    if (!k || !*k)            return "Türkçe";
    if (um_esit(k, "tr"))     return "Türkçe";
    if (um_esit(k, "en"))     return "İngilizce";
    if (um_esit(k, "de"))     return "Almanca";
    if (um_esit(k, "ru"))     return "Rusça";
    if (um_esit(k, "fr"))     return "Fransızca";
    if (um_esit(k, "ar"))     return "Arapça";
    if (um_esit(k, "belirsiz")) return "dili belirsiz";
    return k;
}

/* DAYANAK TÜRKÇE DEĞİLSE SÖYLENİR.
 * NEDEN: kitaplığın %33.4'ü İngilizce (503 parça, 1.14 milyon
 * karakter), neredeyse tamamı "tulga" alanında — Musso ve M104 el
 * kitapları. Kullanıcı Türkçe soruyor; ajan İngilizce bir metinden
 * yanıtlıyorsa bunu BİLDİRMEK zorundadır. Sessizce başka dilde
 * alıntı vermek kaynağı gizlemektir; çeviriyi kendi uydurduğunu
 * saklamak ise daha kötüsü.
 * Çeviri kaydıysa (ozgun_dil dolu) o da söylenir: okuyan, metnin
 * aslının başka dilde olduğunu bilsin. */
static void dil_uyarisi(us_cikti_t *o) {
    int yabanci = 0, ceviri = 0;
    const char *ilk = 0;
    for (int i = 0; i < o->dayanak_sayi; i++) {
        if (o->dayanak[i].ozgun_dil[0]) { ceviri = 1; if (!ilk) ilk = o->dayanak[i].ozgun_dil; }
        else if (!um_esit(o->dayanak[i].dil, "tr")) {
            yabanci = 1; if (!ilk) ilk = o->dayanak[i].dil;
        }
    }
    char n[160];
    if (yabanci) {
        um_bicim(n, sizeof n,
                 "Dayanağım %s bir kaynak; alıntı özgün dilindedir, "
                 "çevirisi benim değildir.", dil_adi(ilk));
        uyari_ekle(o, n);
    } else if (ceviri) {
        um_bicim(n, sizeof n,
                 "Dayanağım %s özgünden çevrilmiş bir kayıttır.", dil_adi(ilk));
        uyari_ekle(o, n);
    }
}


/* Yanıt dayanağa dayanıyor mu? İçerik sözcüklerinin (≥5 bayt) kök öbeği (ilk 6 bayt) ve sayılar dayanakta aranır.
 * Kaynak satırı "Dayanağım: …" yalnız alıntıyı gösterir; yanıtın o alıntıdan çıktığını GARANTİ ETMEZ. Bu denetim
 * modelin kendi bilgisiyle konuştuğu durumu (dayanakta yazmayan bir ad ya da sayı) yakalar. */
int us_dayanak_destegi(const char *yanit, const char *dayanak, int *toplam) {
    int top = 0, bulunan = 0;
    if (toplam) *toplam = 0;
    if (!yanit || !dayanak) return 100;
    uint32_t dn = um_uzun(dayanak), yn = um_uzun(yanit);
    char *d = (char *)up_ayir((uint64_t)dn * 2 + 2), *y = (char *)up_ayir((uint64_t)yn * 2 + 2);
    if (!d || !y) { if (d) up_biraktir(d); if (y) up_biraktir(y); return 100; }
    um_kucult(d, dn * 2 + 2, dayanak);
    um_kucult(y, yn * 2 + 2, yanit);
    const char *p = y;
    while (*p) {
        uint32_t by = 1, kn = um_utf8_oku(p, &by);
        if (!um_kelime_karakteri(kn)) { p += by ? by : 1; continue; }
        const char *bas = p; int rakam = 1;
        while (*p) {
            kn = um_utf8_oku(p, &by);
            if (!um_kelime_karakteri(kn)) break;
            if (kn < '0' || kn > '9') rakam = 0;
            p += by ? by : 1;
        }
        uint32_t uz = (uint32_t)(p - bas);
        if (uz >= 64 || (!rakam && uz < 5) || (rakam && uz < 2)) continue;
        char t[64];
        uint32_t k = rakam ? uz : (uz < 6 ? uz : 6);
        while (!rakam && k > 0 && ((uint8_t)bas[k] & 0xC0) == 0x80 && k < uz) k--;   /* UTF-8 sınırı */
        um_bellek_kopya(t, bas, k); t[k] = 0;
        int w = rakam ? 2 : 1;                       /* sayı daha ağır: uydurulmuş sayı en tehlikelisidir */
        top += w;
        if (um_icinde(d, t)) bulunan += w;
    }
    up_biraktir(d); up_biraktir(y);
    if (toplam) *toplam = top;
    return top ? bulunan * 100 / top : 100;
}

int us_cevap_uret(const us_derle_t *i, us_cikti_t *o) {
    if (!i || !o) return UM_YANLIS;
    um_bellek_doldur(o, 0, sizeof *o);
    o->nereden = i->nereden;
    o->motor = us_motor_adi();

    if (i->uzman_ad && i->uzman_is)
        um_bicim(o->basyazi, sizeof o->basyazi, "%s (%s) söyler:", i->uzman_ad, i->uzman_is);
    uzman_ekle(o, i->uzman_ad);
    uyari_ekle(o, i->uyari);

    for (int b = 0; b < i->baglam_sayi && b < US_DAYANAK_MAX; b++)
        dayanak_ekle(o, &i->baglam[b]);
    dil_uyarisi(o);

    /* çatışma: birleştirme yok, iki kaynak da adıyla anılır */
    if (i->catisma && i->catisma->var) {
        o->catisma = 1;
        um_bicim(o->catisma_notu, sizeof o->catisma_notu,
                 "%s alanında iki ayrı kaynak: %s ile %s",
                 i->catisma->kategori, i->catisma->kaynak_a, i->catisma->kaynak_b);
    }

    /* Dayanak yoksa motor HİÇ çağrılmaz — uydurma kapısı burada kapanır.
     * Önceki görevin çıktısı DAYANAK SAYILMAZ: tek başına bu kapıyı açmaz. */
    int bengi_var = i->bengi_baglam && i->bengi_baglam[0];
    int oncul_var = i->oncul_baglam && i->oncul_baglam[0];
    o->oncul_kullanildi = oncul_var ? 1 : 0;

    if (i->baglam_sayi <= 0 && !bengi_var) {
        o->dayanaksiz = 1;
        o->guven = GUVEN_YOK;
        if (oncul_var)
            um_kopya(o->metin, sizeof o->metin,
                     "Elimde yalnız önceki görevin çıktısı var; onu dayanak saymam. "
                     "Doğrulanmış bir kaynak bulamadığım için sözümü kurmuyorum.\n");
        else
            um_kopya(o->metin, sizeof o->metin,
                     "Bu konuda yeterli dayanak yok. Kütüphanemde de belleğimde de "
                     "buna değen bir kayıt bulamadım; uydurmam.\n");
        return UM_OLDU;
    }

    /* motora verilecek bağlam: önce doğrulanmış hafıza, sonra kaynak alıntıları */
    static char baglam_metni[GOREV_BAGLAM_MAX];
    um_kopya(baglam_metni, sizeof baglam_metni, "");
    if (bengi_var) {
        um_ekle(baglam_metni, sizeof baglam_metni, i->bengi_baglam);
        um_ekle(baglam_metni, sizeof baglam_metni, "\n");
    }
    for (int b = 0; b < i->baglam_sayi; b++) {
        um_ekle(baglam_metni, sizeof baglam_metni, i->baglam[b].alinti);
        um_ekle(baglam_metni, sizeof baglam_metni, "\n");
    }
    /* Öncül görev çıktısı motora gider ama AÇIKÇA etiketlenir; dayanak
     * listesine girmez, güven tavanını yükseltmez. */
    if (oncul_var) {
        um_ekle(baglam_metni, sizeof baglam_metni,
                "[ÖNCEKİ GÖREV ÇIKTISI — doğrulanmamış, kaynak değildir]\n");
        um_ekle(baglam_metni, sizeof baglam_metni, i->oncul_baglam);
        um_ekle(baglam_metni, sizeof baglam_metni, "\n");
    }

    us_istem_t motor_istem = {
        .kisilik   = i->kisilik,
        .baglam    = baglam_metni,
        .soru      = i->soru,
        .niyet     = i->niyet,
        .en_cok_im = i->en_cok_im ? i->en_cok_im : 256
    };

    umay_us_t *m = umay_us_al();
    int sonuc = UM_YANLIS;
    if (m && m->uret) sonuc = m->uret(m, &motor_istem, o->metin, sizeof o->metin);

    /* motor düşerse ya da boş dönerse güvenli yedeğe in */
    if (sonuc != UM_OLDU || !o->metin[0]) {
        umay_us_t *yedek = us_stub_motor();
        if (m != yedek) {
            gun_yaz("US: %s yanıt vermedi, stub yedeğine düştüm.", m ? m->ad : "(motor yok)");
            if (yedek->hazirla) yedek->hazirla(yedek, 0);
            sonuc = yedek->uret(yedek, &motor_istem, o->metin, sizeof o->metin);
            o->motor = yedek->ad;
            o->yedege_dustu = 1;
            uyari_ekle(o, "Asıl çıkarım motoru yanıt vermedi; yedek motorla konuştum.");
        }
        if (sonuc != UM_OLDU || !o->metin[0]) {
            o->dayanaksiz = 1;
            o->guven = GUVEN_YOK;
            um_kopya(o->metin, sizeof o->metin,
                     "Dayanağım vardı ama sözü kuramadım; boş konuşmaktansa susarım.\n");
            return UM_OLDU;
        }
    }

    o->guven = guven_hesapla(i);
    /* Yanıtın özü dayanakta yoksa (model kendi bilgisiyle konuşmuş): güven ZAYIF, açıkça söylenir. */
    if (!o->yedege_dustu && um_icinde(baglam_metni, "") && o->metin[0]) {
        int top = 0, yuzde = us_dayanak_destegi(o->metin, baglam_metni, &top);
        if (top >= 3 && yuzde < 50) {
            o->dayanak_disi = 1;
            if (o->guven > GUVEN_ZAYIF) o->guven = GUVEN_ZAYIF;
            uyari_ekle(o, "Bu yanıtın özü dayanağımda yazmıyor; dil modelinin kendi bilgisi, doğrulanmadı. "
                          "Aşağıdaki kaynaklar konuyu içeriyor ama bu yanıtı doğrulamıyor.");
        }
    }
    /* öncül çıktı işe karıştıysa güven ORTA'yı geçemez */
    if (o->oncul_kullanildi && o->guven > GUVEN_ORTA) o->guven = GUVEN_ORTA;
    return UM_OLDU;
}

/* ---------------- ortak çıktı yazımı ---------------- */

uint32_t us_cikti_yaz(const us_cikti_t *o, char *c, uint32_t n) {
    um_kopya(c, n, "");
    if (o->basyazi[0]) { um_ekle(c, n, o->basyazi); um_ekle(c, n, "\n"); }
    um_ekle(c, n, o->metin);
    if (um_uzun(o->metin) && o->metin[um_uzun(o->metin) - 1] != '\n') um_ekle(c, n, "\n");

    for (int i = 0; i < o->uyari_sayi; i++) {
        um_ekle(c, n, o->uyari[i]);
        if (um_uzun(o->uyari[i]) && o->uyari[i][um_uzun(o->uyari[i]) - 1] != '\n')
            um_ekle(c, n, "\n");
    }

    if (o->nereden == US_ORTAK_HAVUZ)
        um_ekle(c, n, "Kendi alanımda kayıt yoktu; ortak havuzdan baktım.\n");
    else if (o->nereden == US_GENEL_DEPO)
        um_ekle(c, n, "Bu soru benim alanıma düşmedi; genel depoya indim, dayanağım oradan.\n");

    if (o->catisma) {
        char s[576];
        um_bicim(s, sizeof s,
                 "Dikkat: %s. İkisini birleştirmedim; hangisine uyacağını sen seç.\n",
                 o->catisma_notu);
        um_ekle(c, n, s);
    }

    if (o->oncul_kullanildi)
        um_ekle(c, n, "(Önceki görevin çıktısını da okudum — doğrulanmamış, "
                      "dayanak saymadım.)\n");

    for (int i = 0; i < o->dayanak_sayi; i++) {
        char s[352];
        const us_dayanak_t *d = &o->dayanak[i];
        /* Dil, Türkçe olmadığında kaynak satırında da anılır: uyarı
         * yukarıda bir kez geçer, ama HANGİ dayanağın yabancı olduğu
         * ancak burada görünür. */
        char dnot[48];
        dnot[0] = 0;
        if (d->ozgun_dil[0])
            um_bicim(dnot, sizeof dnot, ", %s'den çeviri", dil_adi(d->ozgun_dil));
        else if (!um_esit(d->dil, "tr"))
            um_bicim(dnot, sizeof dnot, ", %s", dil_adi(d->dil));
        um_bicim(s, sizeof s, "(Dayanağım: %s / %s — %s [%s%s])\n",
                 d->kategori, d->baslik, d->kaynak,
                 kaynak_tur_adi(d->kaynak_tur), dnot);
        um_ekle(c, n, s);
    }

    char g[96];
    um_bicim(g, sizeof g, "Güven: %s\n", us_guven_adi(o->guven));
    um_ekle(c, n, g);
    return um_uzun(c);
}

/* ---------------- meclis özeti ---------------- */

void us_ozet_baslat(us_cikti_t *o) {
    um_bellek_doldur(o, 0, sizeof *o);
    o->guven = GUVEN_SAGLAM;      /* en zayıf parçaya göre inecek */
    o->motor = us_motor_adi();
}

void us_ozet_kat(us_cikti_t *o, const us_cikti_t *p) {
    if (!o || !p) return;
    if (p->guven < o->guven) o->guven = p->guven;
    if (p->catisma && !o->catisma) {
        o->catisma = 1;
        um_kopya(o->catisma_notu, sizeof o->catisma_notu, p->catisma_notu);
    }
    if (p->yedege_dustu) o->yedege_dustu = 1;
    if (p->oncul_kullanildi) o->oncul_kullanildi = 1;
    for (int i = 0; i < p->uzman_sayi; i++) uzman_ekle(o, p->uzman[i]);
    for (int i = 0; i < p->uyari_sayi; i++) uyari_ekle(o, p->uyari[i]);
    for (int i = 0; i < p->dayanak_sayi && o->dayanak_sayi < US_DAYANAK_MAX; i++) {
        int var = 0;
        for (int j = 0; j < o->dayanak_sayi; j++)
            if (um_esit(o->dayanak[j].kaynak, p->dayanak[i].kaynak) &&
                um_esit(o->dayanak[j].baslik, p->dayanak[i].baslik)) { var = 1; break; }
        if (!var) o->dayanak[o->dayanak_sayi++] = p->dayanak[i];
    }
    if (p->dayanaksiz && o->uzman_sayi == 1) o->dayanaksiz = 1;
}

uint32_t us_ozet_kuyruk(const us_cikti_t *o, char *c, uint32_t n) {
    char s[256];
    if (o->uzman_sayi > 0) {
        um_kopya(s, sizeof s, "Katılan uzmanlar: ");
        for (int i = 0; i < o->uzman_sayi; i++) {
            if (i) um_ekle(s, sizeof s, ", ");
            um_ekle(s, sizeof s, o->uzman[i]);
        }
        um_ekle(s, sizeof s, "\n");
        um_ekle(c, n, s);
    }
    if (o->catisma) {
        um_bicim(s, sizeof s, "Uzmanlar arasında kaynak çatışması var: %s\n", o->catisma_notu);
        um_ekle(c, n, s);
    }
    um_bicim(s, sizeof s, "Otağın ortak güveni: %s\n", us_guven_adi(o->guven));
    um_ekle(c, n, s);
    return um_uzun(c);
}

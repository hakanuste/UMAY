/* BİLGİ — kütüphane indeksi + bağlam bulma.
 * V9.2 zeka.c'deki kelime çıkarma / kesişim skoru / alıntı pencereleme
 * algoritması korunmuştur; öbek yerine durağan dizi kullanılır. */
#include "umay_bilgi.h"
#include "umay_port.h"
#include "umay_str.h"
#include "umay_ortak.h"

typedef struct {
    char kategori[32];
    char baslik[96];
    char kaynak[96];
    kaynak_tur_t kaynak_tur;
    char dil[BILGI_DIL_MAX];          /* künyeden; boşsa "tr" */
    char ozgun_dil[BILGI_DIL_MAX];    /* çeviri ise özgün dili */
    char etiket[BILGI_ETIKET_MAX][24];
    int  etiket_sayi;
    char icerik[BILGI_ICERIK_MAX];
    char kelime[BILGI_KELIME_MAX][24];
    int  kelime_sayisi;
    int  dolu;
} kayit_t;

static kayit_t g_indeks[BILGI_KAYIT_MAX];
static int     g_sayi;

static const char *DURAK[] = {
    "ve","ile","için","bir","bu","da","de","mi","ne","ki","ya","çok","daha",
    "ama","gibi","olarak","göre","kadar","the","and","for","that","with", 0
};

static int durak_mi(const char *k) {
    if (um_uzun(k) < 3) return 1;
    for (int i = 0; DURAK[i]; i++) if (um_esit(k, DURAK[i])) return 1;
    return 0;
}

/* metinden en çok 'en_cok' benzersiz, küçük harfli kelime çıkarır */
/* SOZCUK AYIRICI — TEK TANE OLMALI.
 * bengi katmani da soru ile kaydin ilgisini olcmek icin buna ihtiyac
 * duyar. Orada ikinci bir ayirici yazilsa durak sozcuk listesi ve
 * Turkce kucultme iki yerde ayri ayri yasar, biri degisince oteki
 * sessizce sapar. Bu yuzden dis ada baglanir. */
int bilgi_kelime_ayikla(const char *metin, char cikan[][24], int en_cok) {
    int n = 0;
    const char *p = metin;
    while (*p && n < en_cok) {
        uint32_t b; uint32_t kn = um_utf8_oku(p, &b);
        if (!um_kelime_karakteri(kn)) { p += b; continue; }
        char kelime[24]; uint32_t o = 0;
        while (*p) {
            kn = um_utf8_oku(p, &b);
            if (!um_kelime_karakteri(kn)) break;
            uint32_t k2 = um_kucuk(kn);
            char yaz[5]; uint32_t yb = um_utf8_yaz(k2, yaz);
            if (o + yb + 1 < sizeof kelime) { um_bellek_kopya(kelime + o, yaz, yb); o += yb; }
            p += b;
        }
        kelime[o] = 0;
        if (durak_mi(kelime)) continue;
        int var = 0;
        for (int i = 0; i < n; i++) if (um_esit(cikan[i], kelime)) { var = 1; break; }
        if (!var) um_kopya(cikan[n++], 24, kelime);
    }
    return n;
}

/* "a,b,c" biçimli etiket dizesini ayırır */
static void etiket_ayikla(kayit_t *k, const char *etiketler) {
    k->etiket_sayi = 0;
    if (!etiketler) return;
    const char *p = etiketler;
    while (*p && k->etiket_sayi < BILGI_ETIKET_MAX) {
        char e[24]; uint32_t o = 0;
        while (*p == ' ' || *p == ',') p++;
        while (*p && *p != ',' && o + 1 < sizeof e) e[o++] = *p++;
        e[o] = 0;
        if (o) um_kopya(k->etiket[k->etiket_sayi++], 24, e);
    }
}

static int kayit_ekle_dil(const char *kategori, const char *baslik, const char *kaynak,
                          kaynak_tur_t kt, const char *etiketler, const char *icerik,
                          const char *dil, const char *ozgun_dil) {
    /* BENGİ kaydı belge deposuna giremez: hafıza ile kaynak karışmasın. */
    if (kt == KT_BENGI) return UM_YANLIS;
    if (g_sayi >= BILGI_KAYIT_MAX) return UM_DOLU;
    kayit_t *k = &g_indeks[g_sayi];
    um_kopya(k->kategori, sizeof k->kategori, kategori);
    um_kopya(k->baslik,   sizeof k->baslik,   baslik);
    um_kopya(k->kaynak,   sizeof k->kaynak,   kaynak ? kaynak : "doktrin");
    k->kaynak_tur = kt;
    /* Dil verilmediyse "tr": eski kayıtlar ve doktrinler Türkçedir. */
    um_kopya(k->dil, sizeof k->dil, (dil && *dil) ? dil : "tr");
    um_kopya(k->ozgun_dil, sizeof k->ozgun_dil, ozgun_dil ? ozgun_dil : "");
    etiket_ayikla(k, etiketler);
    um_kopya(k->icerik,   sizeof k->icerik,   icerik);
    k->kelime_sayisi = bilgi_kelime_ayikla(k->icerik, k->kelime, BILGI_KELIME_MAX);
    k->dolu = 1;
    g_sayi++;
    return UM_OLDU;
}

static int kayit_ekle_tam(const char *kategori, const char *baslik, const char *kaynak,
                          kaynak_tur_t kt, const char *etiketler, const char *icerik) {
    return kayit_ekle_dil(kategori, baslik, kaynak, kt, etiketler, icerik, 0, 0);
}

int bilgi_kayit_kat_tur(const char *kategori, const char *baslik, const char *kaynak,
                        kaynak_tur_t kt, const char *etiketler, const char *icerik) {
    return kayit_ekle_tam(kategori, baslik, kaynak, kt, etiketler, icerik);
}

/* Dili bilinen kayıt (kitaplık yükleyicisi bunu kullanır). */
int bilgi_kayit_dil(const char *kategori, const char *baslik, const char *kaynak,
                    kaynak_tur_t kt, const char *etiketler, const char *icerik,
                    const char *dil, const char *ozgun_dil) {
    return kayit_ekle_dil(kategori, baslik, kaynak, kt, etiketler, icerik,
                          dil, ozgun_dil);
}

int bilgi_kayit_kat(const char *kategori, const char *baslik, const char *kaynak,
                    const char *etiketler, const char *icerik) {
    return kayit_ekle_tam(kategori, baslik, kaynak, KT_DIS, etiketler, icerik);
}

/* ---- depo standardı: 12 uzman alanı + ortak havuz ---- */
static const char *DEPO_DIZIN[BILGI_ALAN_SAYI] = {
    "akca", "yer-su", "kun", "demir", "tulga", "otaci",
    "yargu", "balbal", "alpagu", "kopuz", "oyun", "yazit", "ortak"
};

const char *bilgi_alan_dizini(int i) {
    return (i >= 0 && i < BILGI_ALAN_SAYI) ? DEPO_DIZIN[i] : 0;
}

const char *bilgi_alan_kategorisi(alan_t a) {
    switch (a) {
    case ALAN_AKCA:  return "akca";
    case ALAN_YERSU: return "yer-su";
    case ALAN_KUN:   return "kun";
    case ALAN_DEMIR: return "demir";
    case ALAN_TULGA: return "tulga";
    case ALAN_OTACI: return "otaci";
    case ALAN_YARGU: return "yargu";
    case ALAN_BALBAL:return "balbal";
    case ALAN_ALPAGU:return "alpagu";
    case ALAN_KOPUZ: return "kopuz";
    case ALAN_OYUN:  return "oyun";
    case ALAN_YAZIT: return "yazit";
    default:         return "ortak";
    }
}

alan_t bilgi_kategori_alani(const char *kategori) {
    for (int a = 1; a < ALAN_SAYI; a++)
        if (um_esit(kategori, bilgi_alan_kategorisi((alan_t)a))) return (alan_t)a;
    return ALAN_YOK;
}

int bilgi_kategori_sayisi(const char *kategori) {
    int n = 0;
    for (int i = 0; i < g_sayi; i++)
        if (g_indeks[i].dolu && um_esit(g_indeks[i].kategori, kategori)) n++;
    return n;
}

int bilgi_dizin_kur(const char *kok) {
    if (!kok || !up_yetkim_var_mi(YET_DOSYA_YAZ)) return UM_YETKISIZ;
    int n = 0;
    char yol[UMAY_YOL_MAX];
    up_dizin_yarat(kok);
    for (int i = 0; i < BILGI_ALAN_SAYI; i++) {
        um_bicim(yol, sizeof yol, "%s/%s", kok, DEPO_DIZIN[i]);
        if (up_var_mi(yol) || up_dizin_yarat(yol) == UM_OLDU) n++;
    }
    gun_yaz("Bilgi deposu: %d/%d alan dizini hazır.", n, BILGI_ALAN_SAYI);
    return n;
}

int bilgi_sayi(void) { return g_sayi; }

/* ---------------- 5N1K ---------------- */
static int belirteç_var(char (*t)[24], int n, const char *a) {
    for (int i = 0; i < n; i++) if (um_esit(t[i], a)) return 1;
    return 0;
}

soru_tipi_t bilgi_soru_tipi(const char *soru) {
    if (!soru) return ST_YOK;
    char k[512];
    um_kucult(k, sizeof k, soru);
    char t[24][24];
    int n = 0;
    const char *p = k;
    while (*p && n < 24) {
        while (*p) { uint32_t b; uint32_t kn = um_utf8_oku(p, &b); if (um_kelime_karakteri(kn)) break; p += b ? b : 1; }
        char w[24]; uint32_t o = 0;
        while (*p) {
            uint32_t b; uint32_t kn = um_utf8_oku(p, &b);
            if (!um_kelime_karakteri(kn)) break;
            if (!b) b = 1;
            if (o + b + 1 < sizeof w) { for (uint32_t q = 0; q < b; q++) w[o++] = p[q]; }
            p += b;
        }
        w[o] = 0;
        if (o) um_kopya(t[n++], sizeof t[0], w);
    }
    for (int i = 0; i + 1 < n; i++)
        if (um_esit(t[i], "ne") && (um_esit(t[i + 1], "zaman") || um_esit(t[i + 1], "vakit"))) return ST_NE_ZAMAN;
    if (belirteç_var(t, n, "nerede") || belirteç_var(t, n, "nereye") || belirteç_var(t, n, "nereden") ||
        belirteç_var(t, n, "neresi") || belirteç_var(t, n, "nerelerde")) return ST_NEREDE;
    if (belirteç_var(t, n, "neden") || belirteç_var(t, n, "niçin") || belirteç_var(t, n, "niye") ||
        belirteç_var(t, n, "nedenidir") || belirteç_var(t, n, "sebebi")) return ST_NEDEN;
    if (belirteç_var(t, n, "nasıl") || belirteç_var(t, n, "nasil")) return ST_NASIL;
    if (belirteç_var(t, n, "kim") || belirteç_var(t, n, "kimin") || belirteç_var(t, n, "kimler") ||
        belirteç_var(t, n, "kime") || belirteç_var(t, n, "kimdir")) return ST_KIM;
    if (belirteç_var(t, n, "kaç") || belirteç_var(t, n, "kaçtır")) return ST_KAC;
    for (int i = 0; i + 1 < n; i++)
        if (um_esit(t[i], "ne") && um_esit(t[i + 1], "kadar")) return ST_KAC;
    if (belirteç_var(t, n, "nedir") || belirteç_var(t, n, "ne") || belirteç_var(t, n, "neler") ||
        belirteç_var(t, n, "nelerdir") || belirteç_var(t, n, "anlat") || belirteç_var(t, n, "hangi")) return ST_NE;
    return ST_YOK;
}

const char *bilgi_soru_tipi_adi(soru_tipi_t t) {
    static const char *AD[ST_SAYI] = { "-", "ne", "nerede", "ne zaman", "neden", "nasıl", "kim", "kaç" };
    return (t >= 0 && t < ST_SAYI) ? AD[t] : "-";
}

/* metnin bu soru türüne YANIT verdiğini düşündüren ipuçları */
int bilgi_tip_ipucu_var(soru_tipi_t t, const char *metin) {
    static const char *IP[ST_SAYI][22] = {
        { 0 }, { 0 },
        /* nerede */  { "köy","konum","içinde","altında","üstünde","yanında","bölge","arazi","sınır","kuzey","güney",
                        "doğu","batı","çatı","garaj","kamelya","bahçe","sera","yer altı","toprak altı","cephe", 0 },
        /* ne zaman */{ "zaman","mevsim","kış","yaz","bahar","sonbahar","ay ","gün","saat","yıl","zirve","sabah",
                        "akşam","haftada","her ","önce","sonra","dönem", 0 },
        /* neden */   { "çünkü","için","bu yüzden","sebep","nedeniyle","böylece","amacıyla","sayesinde","korur",
                        "azaltır","önler","sağlar","yoruml", 0 },
        /* nasıl */   { "yapılır","kurulur","uygulanır","önce","sonra","ardından","adım","karıştır","serilir",
                        "işlenir","bağlan","sürülür","beslenir","dönüştürülür","hazırlan","kullanılır","konur",
                        "çıkarılır","üretilir","saklanır", 0 },
        /* kim */     { "kullanıcı","hakan","usta","uzman","kişi","hekim","avukat","eğitmen","dede","sahibi", 0 },
        /* kaç */     { "0","1","2","3","4","5","6","7","8","9","yaklaşık","ortalama","kadar", 0 }
    };
    if (t <= ST_NE || t >= ST_SAYI || !metin) return 1;
    char k[BILGI_ALINTI_MAX + 8];
    um_kucult(k, sizeof k, metin);
    for (int i = 0; IP[t][i]; i++) if (um_icinde(k, IP[t][i])) return 1;
    return 0;
}

int bilgi_konu_listesi(const char *kategori, char (*baslik)[96], int en_cok) {
    int n = 0;
    for (int i = 0; i < g_sayi && n < en_cok; i++) {
        if (!g_indeks[i].dolu || !um_esit(g_indeks[i].kategori, kategori)) continue;
        int var = 0;
        for (int q = 0; q < n; q++) if (um_esit(baslik[q], g_indeks[i].baslik)) var = 1;
        if (!var) um_kopya(baslik[n++], 96, g_indeks[i].baslik);
    }
    return n;
}

/* Dizini ilk n kayda kırpar: sonradan eklenenleri GERİ ALIR (eğitim kapısı). */
void bilgi_kirp(int n) {
    if (n < 0) n = 0;
    while (g_sayi > n) { g_indeks[--g_sayi].dolu = 0; }
}

/* ---- Gömülü doktrin: V15 proje veri tabanı özü ----
 * Kütüphane diskte bulunmasa bile UMAY temel bilgisini taşır. */
static const struct { const char *kat; const char *bas; const char *etiket; const char *ic; } DOKTRIN[] = {
{"ortak","Kullanıcı ve görüş","temel,vizyon",
 "Kullanıcı Hakan Üste, dans eğitmeni. Yaşam görüşü: şebekeden bağımsız (off-grid), kendine yeten, kalıcı, sürdürülebilir, sıfır atıklı, ekolojik yaşam. Yaklaşık 60 yaşında modifiye arazi aracıyla Türkistan ve Orta Asya ata topraklarına yolculuk tasarısı var. Dilde Öz Türkçe sözcükler yeğlenir."},
{"yer-su","Arsa ve permakültür","permakultur,arsa",
 "Aydın Koçarlı Cincin Köyü, 1509 metrekare zeytinlik. Girdi çıktı döngüleri kapatılır. Bölgelendirme Zone 1-5, rüzgar kıranlar, güneş kapanları, çok katmanlı gıda ormanı. Zeytin budama atığından biyokömür biochar üretilir. Ata tohumları vakumlu silika jelli cam kavanozlarda yer altı kilerinde saklanır. Kimyasal girdi kullanılmaz."},
{"balbal","Doğal yapı ve yer altı kileri","yapi,kiler",
 "Dancer's Sanctuary yapımında betonarme ve çimento kesinlikle kullanılmaz. Yerel doğal taş, ahşap, killi toprak ve saman kullanılır. İç duvarlarda nem dengeleyici samanlı toprak sıva uygulanır. Yer altı kileri Horasan harcı bağlayıcılı olup yıl boyu 12-15 derece sabit sıcaklık ve pasif havalandırma bacalarıyla nem dengesi sağlar."},
{"alpagu","Güvenlik ve siber savunma","cevre,siber",
 "Arazi çevresinde off-grid çalışan düşük güç tüketimli gece görüşlü IP kameralar ve Orange Pi tabanlı sınır mikro radar sensörleri bulunur. Daewoo Musso üstünde 360 derece taktik kamera seti ve yakın mesafe engel radarı vardır. Sunucular ve IoT aygıtları internetten bağımsız şifreli yerel ağda çalışır. Kritik veri için air-gapped yedekleme ünitesi kurulur, Wi-Fi jammer saldırısına karşı donanımsal yalıtım uygulanır."},
{"otaci","Acil tıp ve doğal apotek","apotek,ilkyardim",
 "Hastane devre dışıyken ileri ilk yardım, travma müdahalesi, dikiş atma, yara ve kırık dengeleme bilgisi yerel sunucuda tutulur. Serada yetişen kantaron, nane, lavanta, biberiye ve kekikten esansiyel yağ, merhem ve tentür çıkarılır. Koloidal gümüş suyu üretimi ve doğal antiseptik yordamları uygulanır."},
{"ortak","Haritacılık ve pusula","harita,yol",
 "Türkiye için köy yolları, kurumuş dere yatakları, orman patikaları ve topoğrafya katmanlarını içeren en yüksek çözünürlüklü vektörel harita verisi obf ve mbtiles biçiminde yerel diskte tutulur. Gürcistan, Rusya, Kazakistan, Özbekistan, Kırgızistan, Türkmenistan ve Moğolistan için İpek Yolu dağ geçitleri, çöl patikaları ve sınır kapısı koordinatlarını içeren çevrim dışı topoğrafik harita seti bulunur."},
{"ortak","Su yönetimi ve yangın savunması","su,sarnic,yangin",
 "Yağmur suyu hasadı ve yeraltı suyu için Horasan harcı sıvalı toprak altı kadim sarnıç yapılır. Arazi sınırlarına yüksek basınçlı geniş açılı yangın söndürme yağmurlama sistemi kurulur, sarnıçtaki yedek sudan mekanik ya da solar yüksek debili pompayla beslenir."},
{"yer-su","Gölet ekolojisi ve biyolojik arıtma","golet,aritma",
 "Klor ve kimyasal kullanılmadan bitkiler ve yararlı bakterilerle temizlenen yüzme havuzu ve ördek kaz göleti kurulur. Devirdaim pompasıyla su mercimeği, sazlık ve volkanik çakıldan oluşan yenilenme alanından geçirilir. Gambusia balığıyla sivrisinek larvası denetlenir. Gri ve siyah su beş bölümlü hatta katı çökeltme, volkanik taş süzme, sazlık yapay sulak alan, şelaleli oksijenlendirme ve son depolama ile geri kazanılır."},
{"yer-su","Sera, kümes ve biyolojik duvar","kumes,bitki",
 "20 adet Australorp tavuğu akrep ve kene avcısı olarak, 2 veya 3 kaz doğal alarm ve sürüngen itici olarak beslenir. Sınırlarda kadife çiçeği, sarımsak, nane ve pelin otundan keskin kokulu doğal itici duvar oluşturulur."},
{"yer-su","Toprak yenilenmesi","kompost,solucan",
 "Kırmızı Kaliforniya solucanlarıyla mutfak ve kümes atıkları vermikompost olarak işlenir. Dal ve yapraklar sıcak ve soğuk kompostla humuslu toprağa dönüştürülür."},
{"kun","Enerji ve altyapı","solar,batarya",
 "Yaklaşık 6 kW şebekeden bağımsız güneş dizgesi kurulur. Paneller evin çatısına değil, ayrı kamelya ya da garaj yapısının çatısına konur. 3D yazıcının yüksek akım çeken ısıtıcı tablası LiFePO4 bataryaları yormamak için yalnız güneşin zirvede olduğu saatlerde çalıştırılır."},
{"tulga","Mekanik ve otomotiv","motor,sanziman",
 "Proje aracı 1998 model Daewoo Musso. Motor Mercedes-Benz M104 3.2 litre sıralı altı silindir HFM enjeksiyon. Şanzıman Mercedes lisanslı dört ileri otomatik 722.4 veya 722.5 varyantı. Arazi şanzımanı BorgWarner part-time 4WD. BTR şanzımanlar projeden tamamen çıkarılmıştır. Zeytinyağı dip tortusu ve atık yağdan biyodizel üretilir, yakıt hattında Viton hortum kullanılır."},
{"demir","Telsiz ve iletişim","telsiz,anten",
 "HF kısa dalga, VHF UHF, CB 27 MHz, deniz ve hava bandı dinleme altyapısı kurulur. Raspberry Pi üzerinden APRS ve SSTV sayısal veri aktarımı yapılır."},
{"yazit","Umay Kitaplığı akademik arşiv","arsiv,akademik",
 "Tarih ve kültür kaynakları: Ahmet Taşağıl Orta Asya Göktürkler ve Türkistan göç yolları, İlber Ortaylı Türk yönetim tarihi ve kültür coğrafyası, Celal Şengör Orta Asya tektoniği ve yer bilimleri. Sanat ve dil kaynakları: Metin And geleneksel Türk tiyatrosu şamanik törenler ve dans antropolojisi, Refik Ahmet Sevengil sahne sanatları tarihi, Kazım Mirşan proto Türk yazıtları ve runik tamgalar, Muazzez İlmiye Çığ Sümer ve Mezopotamya kültür sürekliliği."},
{"ortak","Çarklar mekaniği ve kinetik enerji","mekanik,disli",
 "Temel kural hızdan feragat et güce tahvil et, mekanik kazanç 10 üzeri. Beş altı dişliden oluşan kaydırmalı vites sliding gear şanzıman kutusu tasarlanır. Mod A kuvvet öğütme: yel değirmeni ya da koşu tamburundan gelen hareket kademeli redüksiyonla yüksek torka çevrilir, ağır bazalt taş değirmeni döndürerek bakliyatı un yapar. Mod B hız hızlı şarj: küçük el çarkı altı kademeli hız artırıcı overdrive dişli zinciri, bilyalı rulman ve ağır volan tekerleği üzerinden alternatörü yüksek devire ulaştırır, günde bir saatlik çevrimle aküler beslenir. Ana mil hattından tahrik alan bağımsız darbe üreteci çit hatlarına yüksek voltaj düşük amper vererek öldürmeyen caydırıcılık sağlar. Arazi kapıları sonsuz vida ve dişli çark worm gear ile çalışır, sistem geri dönüşsüz self locking olduğundan kapı dışarıdan itilerek açılamaz, gemi dümeni tipi el çarkı ve kontr ağırlıkla tonluk kapılar el gücüyle açılır."},
{"oyun","Dans, koreografi ve sahne","dans,sahne",
 "Kullanıcı dans eğitmenidir. Sığınakta prova alanı, düz ve kaymaz döşeme, yeterli tavan yüksekliği ve doğal aydınlatma gözetilir. Metin And'ın geleneksel Türk tiyatrosu, şamanik törenler ve dans antropolojisi çalışmaları ile Refik Ahmet Sevengil'in sahne sanatları tarihi kaynakları arşivde tutulur. Halay, zeybek, horon ve semah gibi yerel oyunların adım ve dizilim çizimleri belgelenir."},
{"kopuz","Ezgi, makam ve çalgı","muzik,calgi",
 "Kopuz, davul ve saz gibi çalgılar şebekeden bağımsız ortamda elektrik gerektirmeden çalınabilir. Makam ve usul bilgisi ile ezgi kayıtları sayısal olarak yerel arşivde tutulur. Dans provaları için ritim ve tempo kaynakları oyun alanıyla birlikte düzenlenir."},
{"yazit","Arşiv ve belge düzeni","arsiv,belge",
 "Umay Kitaplığı kaynakları kategori dizinlerinde düz metin olarak tutulur. Her belgeye künye, tarih ve kaynak bilgisi eklenir. Kritik veri için air-gapped yedekleme ünitesi kullanılır, iki ayrı ortamda kopya bulundurulur. Belge adları aranabilir sözcükler içerir."},
{"yargu","Yasa ve mevzuat çerçevesi","mevzuat,imar",
 "Zeytinlik vasıflı tarım arsasında yapılaşma imar mevzuatına ve zeytinciliğin ıslahı hakkındaki düzenlemelere bağlıdır. Yapı ruhsatı, tapu kaydı, kadastro sınırı ve su kuyusu izni gibi başlıklar ilgili kurumdan doğrulanmalıdır. Bu bilgi genel çerçevedir, hukuki görüş değildir."},
{"akca","Akçe ve maliyet düzeni","maliyet,piyasa",
 "Yatırım kalemleri güneş dizgesi, sarnıç, kiler, yapı malzemesi ve araç bakımı olarak ayrılır. Piyasa, döviz ve altın gibi anlık veriler ancak ULAK ağ bağlantısı açıkken alınabilir; ağ yokken güncel sayı verilmez. Maliyet karşılaştırmaları yerel ve ikinci el kaynak önceliğiyle yapılır."},
{"demir","Elektronik ve donanım","elektronik,kart",
 "Sınır radar sensörleri ve kameralar Orange Pi tabanlı kartlarla sürülür. Kartlar düşük güç tüketimi için uyku kipinde tutulur, besleme hattı LiFePO4 bataryadan gerilim düşürücü üzerinden alınır. Raspberry Pi telsiz tarafında APRS ve SSTV sayısal veri aktarımında kullanılır. Ölçüm yapılmadan devreye gerilim verilmez."},
{0,0,0,0}
};

int bilgi_doktrin_yukle(void) {
    int n = 0;
    for (int i = 0; DOKTRIN[i].kat; i++)
        if (kayit_ekle_tam(DOKTRIN[i].kat, DOKTRIN[i].bas, "doktrin", KT_DOKTRIN,
                           DOKTRIN[i].etiket, DOKTRIN[i].ic) == UM_OLDU) n++;
    return n;
}

/* ---- belge üstverisi ----
 * Kategori DOSYA YOLUNDAN çıkarılmaz; belgenin başındaki satırlar esastır:
 *   #kategori: balbal
 *   #baslik:   Horasan harcı
 *   #etiket:   harc,sarnic
 *   #kaynak:   Usta notları
 * Üstveri yoksa belge "ortak" havuzuna düşer ve günlüğe not edilir. */
static const char *ustveri_al(const char *bas, const char *anahtar, char *c, uint32_t n) {
    const char *p = bas;
    c[0] = 0;
    while (*p == '#') {
        const char *satir_sonu = p;
        while (*satir_sonu && *satir_sonu != '\n') satir_sonu++;
        if (um_bas_mi(p + 1, anahtar)) {
            const char *v = p + 1 + um_uzun(anahtar);
            while (*v == ':' || *v == ' ' || *v == '\t') v++;
            uint32_t o = 0;
            while (v < satir_sonu && o + 1 < n) c[o++] = *v++;
            while (o > 0 && (c[o-1] == ' ' || c[o-1] == '\r')) o--;
            c[o] = 0;
            return c;
        }
        if (!*satir_sonu) break;
        p = satir_sonu + 1;
    }
    return 0;
}

/* üstveri başlığından sonraki gövdenin başlangıcı */
static const char *govde_bul(const char *bas) {
    const char *p = bas;
    while (*p == '#') {
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
    }
    while (*p == '\n' || *p == '\r') p++;
    return p;
}

static int gecerli_kategori(const char *k) {
    for (int i = 0; i < BILGI_ALAN_SAYI; i++) if (um_esit(k, DEPO_DIZIN[i])) return 1;
    return 0;
}

/* Bir belgeyi üstverisine göre indeksler. */
static int belge_yukle(const char *yol) {
    static char tampon[BILGI_ICERIK_MAX];
    up_tut_t t = up_ac(yol, UP_OKU);
    if (t < 0) return UM_YOK;
    int64_t okundu = up_oku(t, tampon, sizeof tampon - 1);
    up_kapat(t);
    if (okundu <= 0) return UM_YOK;
    tampon[okundu] = 0;

    char kategori[32], baslik[96], etiket[96], kaynak[96];
    char dil[BILGI_DIL_MAX], ozgun[BILGI_DIL_MAX];
    const char *k = ustveri_al(tampon, "kategori", kategori, sizeof kategori);
    const char *b = ustveri_al(tampon, "baslik",   baslik,   sizeof baslik);
    const char *e = ustveri_al(tampon, "etiket",   etiket,   sizeof etiket);
    const char *s = ustveri_al(tampon, "kaynak",   kaynak,   sizeof kaynak);
    /* #dil ve #ozgun_dil: kitaplığın %33.4'ü İngilizce. Künyede yoksa
     * "tr" varsayılır (eski kayıtlar ve doktrinler Türkçedir). */
    if (!ustveri_al(tampon, "dil", dil, sizeof dil) || !dil[0])
        um_kopya(dil, sizeof dil, "tr");
    if (!ustveri_al(tampon, "ozgun_dil", ozgun, sizeof ozgun)) ozgun[0] = 0;

    if (!k || !gecerli_kategori(kategori)) {
        gun_yaz("Belge üstverisiz ya da tanınmaz kategoride, ortak havuza koydum: %s", yol);
        um_kopya(kategori, sizeof kategori, "ortak");
    }
    if (!b || !baslik[0]) um_kopya(baslik, sizeof baslik, yol);
    if (!e) etiket[0] = 0;
    /* kaynak adı verilmişse yol ile birlikte saklanır */
    char kaynak_tam[96];
    if (s && kaynak[0]) um_bicim(kaynak_tam, sizeof kaynak_tam, "%s (%s)", yol, kaynak);
    else um_kopya(kaynak_tam, sizeof kaynak_tam, yol);

    /* "web-" ile başlayan dosyayı ÖĞREN modülü yazar: doğrulanmamış
     * dış kayıttır, elle konmuş belge gibi güvenilmez. */
    kaynak_tur_t tur = KT_BELGE;
    { const char *ad = yol; for (const char *q = yol; *q; q++) if (*q == '/' || *q == '\\') ad = q + 1;
      if (um_bas_mi(ad, "web-") || um_bas_mi(ad, "ceviri-")) tur = KT_DIS; }
    return kayit_ekle_dil(kategori, baslik, kaynak_tam, tur, etiket,
                          govde_bul(tampon), dil, ozgun);
}

int bilgi_belge_ekle(const char *yol) { return belge_yukle(yol); }

/* Depoyu tarar: 13 alan dizini + kök. Kategori yine üstveriden gelir;
 * dizin adı yalnız nereye bakılacağını söyler, kategoriyi belirlemez. */
int bilgi_tara(const char *kok) {
    if (!kok || !up_yetkim_var_mi(YET_DOSYA_OKU)) return 0;
    int n = 0;
    char yol[UMAY_YOL_MAX];
    up_girdi_t girdi[32];

    /* GERÇEK TARAMA: her alan dizini LİSTELENİR.
     *
     * Eskiden dizin hiç okunmuyordu; her alanda altı SABİT dosya adı
     * (belge1.txt, notlar.txt, kaynak.txt ...) yoklanıyordu. Yani
     * kütüphaneye konan bir belge, künyesi doğru olsa bile görünmezdi
     * — alan başına en çok altı, o da ancak adı tutarsa. Gerçek bir
     * arşivde dosya adları böyle olmaz.
     *
     * ÖTÜKEN'de SYS_LISTDIR baştan beri vardı; port onu dışa
     * vurmamıştı. Şimdi vuruyor. */
    for (int d = 0; d <= BILGI_ALAN_SAYI && g_sayi < BILGI_KAYIT_MAX; d++) {
        const char *dizin = (d < BILGI_ALAN_SAYI) ? DEPO_DIZIN[d] : 0;
        const char *ayrac = (kok[0] && kok[um_uzun(kok) - 1] == '/') ? "" : "/";

        char dyol[UMAY_YOL_MAX];
        if (dizin) um_bicim(dyol, sizeof dyol, "%s%s%s", kok, ayrac, dizin);
        else       um_bicim(dyol, sizeof dyol, "%s", kok);

        for (int sayfa = 0; sayfa < 4096 && g_sayi < BILGI_KAYIT_MAX;
             sayfa += 32) {
        int adet = up_dizin_listele(dyol, girdi, sayfa, 32);
        if (adet <= 0) break;

        for (int i = 0; i < adet && g_sayi < BILGI_KAYIT_MAX; i++) {
            if (girdi[i].dizin_mi) continue;
            /* yalnız düz metin: .txt ve .md */
            uint32_t u = um_uzun(girdi[i].ad);
            int metin = (u > 4 && um_esit(girdi[i].ad + u - 4, ".txt")) ||
                        (u > 3 && um_esit(girdi[i].ad + u - 3, ".md"));
            if (!metin) continue;
            /* model ve sözlük belge değildir */
            if (um_esit(girdi[i].ad, "umay.us1") ||
                um_esit(girdi[i].ad, "sozluk.txt") ||
                um_esit(girdi[i].ad, "bengi.txt") || um_esit(girdi[i].ad, "ozturkce.txt") ||
                um_esit(girdi[i].ad, "ayar.txt")) continue;

            um_bicim(yol, sizeof yol, "%s/%s", dyol, girdi[i].ad);
            if (belge_yukle(yol) == UM_OLDU) n++;
        }
        if (adet < 32) break;          /* son sayfa */
        }
    }
    if (g_sayi >= BILGI_KAYIT_MAX)
        gun_yaz("Kütüphane: indeks DOLDU (%d kayıt) - kalan belgeler "
                "okunmadı.", BILGI_KAYIT_MAX);
    gun_yaz("Kütüphane: %d belge indekslendi, toplam %d kayıt.", n, g_sayi);
    return n;
}


/* SÖZCÜK LİSTESİ YETMEZ, İÇERİĞE DE BAKILIR.
 *
 * ÖLÇÜLDÜ: BILGI_KELIME_MAX 96'dır, ama 2600 baytlık bir parçada
 * ortanca 215 AYRI sözcük var (en çok 401). Parçaların %95,5'inde
 * sözcük dağarcığının bir kısmı aramaya HİÇ GÖRÜNMÜYORDU.
 *
 * Somut sonuç: "horasan harcı karışımı nedir" sorusunda kazanan parça
 * yer-su/b0802018 idi ve içinde "horasan", "harç", "harcı" HİÇ
 * geçmiyor — yalnız "karışım" tutuyordu. Oysa balbal'da horasan geçen
 * 17 parça var; o parçada "harç" 102., "karışım" 132. ayrı sözcük,
 * yani ikisi de 96 sınırının dışındaydı. Soru doğru alana gitmiyordu.
 *
 * Sınırı 320'ye çıkarmak kapsamı %99,4'e taşır ama sözcük tablosunu
 * 9,4 MB'tan 31,5 MB'a çıkarır — ÖTÜKEN'in Ring3 belleğinde kabul
 * edilemez. İçerik ZATEN kayıtta duruyor: liste tutmazsa içerik
 * taranır. Ek bellek SIFIR; bedel yalnızca tutmayan sözcük için bir
 * tarama ve o da liste DOLMUŞSA yapılır.
 *
 * Tam sözcük aranır: "harç" ararken "harçlık" tutmasın. */
/* Metindeki sıradaki sözcüğü küçük harfli olarak verir; sonunu
 * döndürür, bitince 0. Durak ayıklaması YAPMAZ. */
static const char *sonraki_sozcuk(const char *p, char kelime[24])
{
    if (!p) return 0;
    uint32_t o = 0;
    kelime[0] = 0;
    /* sözcük başına kadar atla */
    while (*p) {
        uint32_t b = 0, kn = um_utf8_oku(p, &b);
        if (!kn || !b) return 0;
        if (um_kelime_karakteri(um_kucuk(kn))) break;
        p += b;
    }
    if (!*p) return 0;
    while (*p) {
        uint32_t b = 0, kn = um_utf8_oku(p, &b);
        if (!kn || !b) break;
        uint32_t kk = um_kucuk(kn);
        if (!um_kelime_karakteri(kk)) break;
        char gec[8];
        uint32_t gb = um_utf8_yaz(kk, gec);
        if (o + gb + 1 >= 24) { p += b; continue; }   /* uzun sözcük kırpılır */
        for (uint32_t i = 0; i < gb; i++) kelime[o++] = gec[i];
        p += b;
    }
    kelime[o] = 0;
    return o ? p : 0;
}

static int icerikte_tam_sozcuk(const char *icerik, const char *kw)
{
    uint32_t kn = um_uzun(kw);
    if (!kn || !icerik) return 0;
    for (const char *p = icerik; *p; p++) {
        if (p != icerik) {
            uint32_t b = 0, onceki = um_utf8_oku(p - 1, &b);
            /* Çok baytlı imin ortasına denk gelmemek için: önceki bayt
             * sözcük karakteriyse bu konum sözcük başı değildir. */
            if (um_kelime_karakteri(um_kucuk(onceki))) continue;
        }
        uint32_t i = 0, j = 0;
        int tutar = 1;
        while (j < kn) {
            uint32_t ab = 0, bb = 0;
            uint32_t x = um_utf8_oku(p + i, &ab);
            uint32_t y = um_utf8_oku(kw + j, &bb);
            if (!x || um_kucuk(x) != um_kucuk(y)) { tutar = 0; break; }
            i += ab; j += bb;
        }
        if (tutar) {
            uint32_t sb = 0, sonraki = um_utf8_oku(p + i, &sb);
            if (!sonraki || !um_kelime_karakteri(um_kucuk(sonraki))) return 1;
        }
    }
    return 0;
}

/* TÜRKÇE EKLİ SÖZCÜK EŞLEMESİ
 *
 * ÖLÇÜLDÜ: eşleme tam eşitlikti (um_esit) ve Türkçenin eklemeli yapısı
 * onu bozuyordu. Kütüphanede geçiş sayıları:
 *       telsiz 38 parça   /  telsizde   0 parça
 *       yazıt   3 parça   /  yazıtları 1 parça
 *       harç   21 parça   /  harcı     16 parça
 * Soru "telsizde SWR değeri ne olmalı" olduğunda ayıklanan sözcükler
 * telsizde(0), swr(0), değeri(1), olmalı(0) — yani HİÇBİR ayırt edici
 * sözcük tutmuyor ve soru rastgele bir alana düşüyor. Ekler elle
 * çıkarılıp "telsiz SWR değer" sorulduğunda doğru alana (demir)
 * gidiyordu: kusurun eşlemede olduğu böyle doğrulandı.
 *
 * Kural (ÖTÜKEN ağacındaki kanıt katmanının belgelediği ölçüt):
 *   - aynı sözcük;
 *   - ikisi de >= 5 harfse ilk 5 harf aynı;
 *   - kısa olan 3-4 harfse, uzun olan onunla başlayıp en çok 2 harflik
 *     ve ünlüyle başlayan bir ek alıyorsa.
 * Son kök harfinde ünsüz yumuşaması serbest: harç~harcı, kireç~kireci,
 * kitap~kitabı, ahenk~ahengi.
 *
 * HARF sayılır, BAYT değil: Türkçe imler UTF-8'de çok baytlıdır,
 * "ilk 5 bayt" demek "ilk 2-3 harf" demek olurdu. */
static uint32_t yumusak(uint32_t h)
{
    switch (h) {
    case 0x00E7: return 'c';          /* ç -> c */
    case 'p':    return 'b';
    case 't':    return 'd';
    case 'k':    return 0x011F;       /* k -> ğ */
    default:     return h;
    }
}

/* Sözcüğü harflere ayırır; en çok n harf. Harf sayısını döndürür. */
static int harflere(const char *s, uint32_t *h, int n)
{
    int k = 0;
    while (*s && k < n) {
        uint32_t b = 0;
        uint32_t kn = um_utf8_oku(s, &b);
        if (!kn || !b) break;
        h[k++] = um_kucuk(kn);
        s += b;
    }
    return k;
}

static int unlu_mu(uint32_t h)
{
    static const uint32_t U[] = { 'a','e',0x0131,'i','o','u',
                                  0x00F6, 0x00FC };   /* ö ü */
    for (unsigned i = 0; i < sizeof U / sizeof U[0]; i++)
        if (h == U[i]) return 1;
    return 0;
}

static int sozcuk_esles(const char *sorgu, const char *kayit)
{
    if (um_esit(sorgu, kayit)) return 1;

    uint32_t a[32], b[32];
    int na = harflere(sorgu, a, 32), nb = harflere(kayit, b, 32);
    if (na < 3 || nb < 3) return 0;

    const uint32_t *kisa = a; int nk = na;
    const uint32_t *uzun = b; int nu = nb;
    if (nb < na) { kisa = b; nk = nb; uzun = a; nu = na; }

    /* Önek karşılaştırması; son kök harfinde yumuşama serbest.
     *
     * YUMUŞAMA EN AZ 4 HARFLİK KÖKTE: üç harflik sözcükte bu kural
     * yanlış eşleşme üretiyordu. ÖLÇÜLDÜ — İngilizce "and", Türkçe
     * "anten" sorgusuyla eşleşiyordu:
     *     a-n-d ~ a-n-t  (son harfte d->t yumuşaması serbest)
     *     kısa 3 harf, ek 2 harf ("en"), ünlüyle başlıyor -> eşleşti
     * Sonuç: "anten" sorgusunda üç dayanağın üçü de tıbbi bitkiler ve
     * kompost belgelerinden geliyordu; sözcüğün gerçekten geçtiği 31
     * parçanın (demir, alpagu) hiçbiri değil.
     *
     * Eşik 4'e çıkarıldı ve gerçek Türkçe çiftlerle sınandı: harç~harcı,
     * kitap~kitabı, ağaç~ağacı, renk~rengi, kök~kökü, kerpiç~kerpici,
     * toprak~toprağı — hepsi tutmaya devam ediyor. */
    int ortak = (nk < 5) ? nk : 5;
    for (int i = 0; i < ortak; i++) {
        if (kisa[i] == uzun[i]) continue;
        /* yalnız kısa sözcüğün SON harfinde, 4+ harflik kökte */
        if (i == nk - 1 && nk >= 4 &&
            yumusak(kisa[i]) == yumusak(uzun[i])) continue;
        return 0;
    }
    if (nk >= 5) return 1;                 /* ilk 5 harf aynı */

    /* kısa 3-4 harf: ek en çok 2 harf ve ÜNLÜYLE başlamalı
     * (kök+ünsüz birleşmesi ayrı sözcük olabilir: "kan" ~ "kanca" değil) */
    int ek = nu - nk;
    if (ek < 1 || ek > 2) return 0;
    return unlu_mu(uzun[nk]);
}

static int kayit_tasiyor(const kayit_t *r, const char *kw)
{
    /* Hızlı yol: kapaklı sözcük listesi (küçük harfli saklanır). */
    for (int b = 0; b < r->kelime_sayisi; b++)
        if (sozcuk_esles(kw, r->kelime[b])) return 1;
    /* Liste DOLDUYSA geri kalan dağarcık görünmez; içeriğe bakılır. */
    if (r->kelime_sayisi >= BILGI_KELIME_MAX) {
        if (icerikte_tam_sozcuk(r->icerik, kw)) return 1;
        /* İçerikte de ekli biçimle arama: sözcük sözcük gez. */
        char k[24];
        const char *p = r->icerik;
        while ((p = sonraki_sozcuk(p, k)) != 0)
            if (sozcuk_esles(kw, k)) return 1;
    }
    return 0;
}

/* V9.2 baglam_bul() ile aynı: kesişim skoru + ilk eşleşme çevresinde pencere.
 * kategoriler verilirse yalnız o kategorilerdeki kayıtlara bakılır. */
int bilgi_baglam_bul_suzgec(const char *soru, const bilgi_suzgec_t *sz,
                            baglam_t *cikan, int en_cok) {
    const char *const *kategoriler = sz ? sz->kategoriler : 0;
    int kat_sayi = sz ? sz->kategori_sayi : 0;
    char kw[24][24];
    int kws = bilgi_kelime_ayikla(soru, kw, 24);
    if (kws == 0 || en_cok <= 0) return 0;

    /* SEYREKLİK AĞIRLIĞI (IDF)
     *
     * Puan eskiden "sorgu sözcüklerinden kaçı kayıtta geçiyor"
     * sayısıydı: ağırlık yok, sıklık yok, uzunluk normalizasyonu yok.
     * Üç sözcüklü bir sorguda yüzlerce kayıt 1 puanda eşitleniyor ve
     * sıralama eşitlikte İNDEKS SIRASINA düşüyordu — yani rastgele.
     * ÖLÇÜLDÜ: "telsiz anten empedans" sorusunda üç dayanağın üçü de
     * "oyun" alanından geldi, halbuki "telsiz" tek başına doğru
     * alanı (demir) buluyordu.
     *
     * Artık her sorgu sözcüğü, kütüphanede NE KADAR SEYREK olduğuna
     * göre ağırlık taşır: tamsayı log2(kayıt/belge_sıklığı) + 1.
     * "telsiz" 33 kayıtta -> ağır; "and" gibi her yerde geçen bir
     * sözcük -> 1. Böylece bir sözcüğü tutan zayıf bir kayıt, üç
     * sözcüğü tutan kaydı geçemez.
     *
     * Tamsayı aritmetiği: ÖTÜKEN'de kayan nokta yok. */
    int agirlik[24];
    for (int a = 0; a < kws; a++) {
        int df = 0;
        for (int i = 0; i < g_sayi; i++)
            if (kayit_tasiyor(&g_indeks[i], kw[a])) df++;
        int w = 1;
        for (int n = g_sayi / (df + 1); n > 1; n >>= 1) w++;
        agirlik[a] = w;
    }

    int skor[BILGI_KAYIT_MAX], sira[BILGI_KAYIT_MAX], es = 0;
    for (int i = 0; i < g_sayi; i++) {
        if (kategoriler && kat_sayi > 0) {
            int uygun = 0;
            for (int q = 0; q < kat_sayi; q++)
                if (kategoriler[q] && um_esit(g_indeks[i].kategori, kategoriler[q])) { uygun = 1; break; }
            if (!uygun) continue;
        }
        if (sz && sz->etiket) {
            int uygun = 0;
            for (int q = 0; q < g_indeks[i].etiket_sayi; q++)
                if (um_esit(g_indeks[i].etiket[q], sz->etiket)) { uygun = 1; break; }
            if (!uygun) continue;
        }
        if (sz && sz->kaynak && !um_icinde(g_indeks[i].kaynak, sz->kaynak)) continue;
        int ortak = 0;
        for (int a = 0; a < kws; a++)
            if (kayit_tasiyor(&g_indeks[i], kw[a])) ortak += agirlik[a];
        if (ortak > 0) { skor[es] = ortak; sira[es] = i; es++; }
    }
    for (int i = 0; i < es; i++)
        for (int j = i + 1; j < es; j++)
            if (skor[j] > skor[i]) {
                int ts = skor[i]; skor[i] = skor[j]; skor[j] = ts;
                int ti = sira[i]; sira[i] = sira[j]; sira[j] = ti;
            }

    int sonuc = es < en_cok ? es : en_cok;
    static char kucuk[BILGI_ICERIK_MAX];
    for (int i = 0; i < sonuc; i++) {
        kayit_t *r = &g_indeks[sira[i]];
        um_kopya(cikan[i].kategori, sizeof cikan[i].kategori, r->kategori);
        um_kopya(cikan[i].baslik,   sizeof cikan[i].baslik,   r->baslik);
        um_kopya(cikan[i].kaynak,   sizeof cikan[i].kaynak,   r->kaynak);
        cikan[i].kaynak_tur = r->kaynak_tur;
        um_kopya(cikan[i].dil, sizeof cikan[i].dil, r->dil);
        um_kopya(cikan[i].ozgun_dil, sizeof cikan[i].ozgun_dil, r->ozgun_dil);
        cikan[i].etiket_sayi = r->etiket_sayi;
        for (int e = 0; e < r->etiket_sayi; e++)
            um_kopya(cikan[i].etiket[e], 24, r->etiket[e]);
        cikan[i].skor = skor[i];

        um_kopya(kucuk, sizeof kucuk, r->icerik);
        um_kucult_esbicim(kucuk);

        /* ALINTI CÜMLESİ: soruyla EN ÇOK anahtar sözcük paylaşan cümlede başlar.
         * Eskiden ilk eşleşen sözcüğün çevresi alınıyordu; "kil" kökü "killi"
         * sözcüğünde tuttuğu için kiler sorusu kayıdın başındaki ilgisiz
         * cümleyle yanıtlanıyordu (kayıtta 12-15 derece bilgisi vardı). */
        int32_t uz = (int32_t)um_uzun(r->icerik), bas = 0, bit;
        int32_t en_iyi = -1, en_skor = 0, cs = 0;
        soru_tipi_t tip = bilgi_soru_tipi(soru);
        while (cs < uz) {
            int32_t ce = cs;
            while (ce < uz && !(r->icerik[ce] == '.' && (ce + 1 >= uz || r->icerik[ce + 1] == ' ' || r->icerik[ce + 1] == '\n'))) ce++;
            char son = kucuk[ce < uz ? ce : uz];
            kucuk[ce < uz ? ce : uz] = 0;
            int sk = 0;
            for (int a = 0; a < kws; a++) if (um_icinde(kucuk + cs, kw[a])) sk++;
            kucuk[ce < uz ? ce : uz] = son;
            /* 5N1K: soru türünün ipucunu taşıyan cümle eşitlikte öne geçer
             * ("nasıl" → yapılır/önce/sonra, "ne zaman" → mevsim/ay/saat ...) */
            if (sk > 0) {
                char son2 = r->icerik[ce < uz ? ce : uz];
                char tmp[BILGI_ALINTI_MAX + 8];
                int32_t cl = (ce < uz ? ce : uz) - cs; if (cl > (int32_t)sizeof tmp - 1) cl = (int32_t)sizeof tmp - 1;
                um_bellek_kopya(tmp, r->icerik + cs, (uint64_t)cl); tmp[cl] = 0; (void)son2;
                sk = sk * 2 + (bilgi_tip_ipucu_var(tip, tmp) ? 1 : 0);
            }
            if (sk > en_skor) { en_skor = sk; en_iyi = cs; }
            cs = ce + 1;
            while (cs < uz && (r->icerik[cs] == ' ' || r->icerik[cs] == '\n')) cs++;
        }
        if (en_iyi < 0) { bas = 0; bit = uz < 600 ? uz : 600; }
        else { bas = en_iyi; bit = bas + 600; if (bit > uz) bit = uz; }
        /* sözcük ortasından başlamasın: sonraki boşluğa kaydır (cümle başında gerekmez) */
        if (bas > 0 && en_iyi < 0) {
            int32_t k = bas;
            while (k < bit && r->icerik[k] != ' ' && k - bas < 40) k++;
            if (k < bit) bas = k + 1;
        }
        int32_t parca = bit - bas;
        if (parca > BILGI_ALINTI_MAX - 8) parca = BILGI_ALINTI_MAX - 8;
        um_bellek_kopya(cikan[i].alinti, r->icerik + bas, (uint64_t)parca);
        cikan[i].alinti[parca] = 0;
    }
    return sonuc;
}

int bilgi_baglam_bul_kat(const char *soru, const char *const *kategoriler,
                         int kat_sayi, baglam_t *cikan, int en_cok) {
    bilgi_suzgec_t sz = { kategoriler, kat_sayi, 0, 0 };
    return bilgi_baglam_bul_suzgec(soru, &sz, cikan, en_cok);
}

int bilgi_kayit_oku(int i, bilgi_ham_t *k) {
    if (!k || i < 0 || i >= g_sayi || !g_indeks[i].dolu) return UM_YOK;
    const kayit_t *r = &g_indeks[i];
    k->kategori = r->kategori; k->baslik = r->baslik; k->kaynak = r->kaynak;
    k->dil = r->dil[0] ? r->dil : "tr"; k->icerik = r->icerik; k->kaynak_tur = r->kaynak_tur;
    return UM_OLDU;
}

int bilgi_baglam_bul(const char *soru, baglam_t *cikan, int en_cok) {
    return bilgi_baglam_bul_suzgec(soru, 0, cikan, en_cok);
}

/* ---------------------------------------------------------------------
 * ÇATIŞMA — aynı kategoride, AYRI kaynaklardan gelen yakın skorlu iki
 * kayıt bulunduğunda bunlar sessizce birleştirilmez; çağırana bildirilir.
 * Bu bir anlam denetimi değildir: iki ayrı kaynağın aynı konuda söz
 * söylediğini saptar ve kararı kullanıcıya bırakır.
 * ------------------------------------------------------------------ */
/* ÇATIŞMA AYNI BELGENİN İKİ PARÇASI ARASINDA OLMAZ.
 *
 * kaynak alanı "kut/yer-su/b0802018.txt (02_KompostRehber_web.txt)"
 * biçimindedir: parça yolu + parantez içinde ÖZGÜN belge. Çatışma
 * denetimi bütün dizgeyi karşılaştırıyordu, dolayısıyla tek bir
 * belgenin 18. ve 47. parçası "iki ayrı kaynak" sayılıyor ve her
 * çok parçalı yanıtta "Dikkat: iki ayrı kaynak" uyarısı çıkıyordu.
 * Ölçülen örnek: "kompost nedir" sorusunda üç dayanağın üçü de
 * 02_KompostRehber_web.txt'ti ve yine çatışma bildirildi.
 *
 * Her yanıtta çıkan bir uyarı, uyarı olmaktan çıkar: kullanıcı onu
 * görmezden gelmeyi öğrenir ve GERÇEK çatışmayı da kaçırır. Artık
 * özgün belge adları karşılaştırılır. */
static void kaynak_koku(const char *k, char *c, uint32_t n) {
    c[0] = 0;
    if (!k) return;
    /* son parantez çiftinin içi */
    const char *ac = 0, *kapa = 0;
    for (const char *p = k; *p; p++) {
        if (*p == '(') ac = p + 1;
        else if (*p == ')' && ac) kapa = p;
    }
    if (ac && kapa && kapa > ac) {
        uint32_t o = 0;
        for (const char *p = ac; p < kapa && o + 1 < n; p++) c[o++] = *p;
        c[o] = 0;
        return;
    }
    um_kopya(c, n, k);
}

int bilgi_catisma_bul(const baglam_t *dizi, int sayi, catisma_t *c) {
    if (c) um_bellek_doldur(c, 0, sizeof *c);
    if (!dizi || sayi < 2) return 0;
    for (int i = 0; i < sayi; i++)
        for (int j = i + 1; j < sayi; j++) {
            if (!um_esit(dizi[i].kategori, dizi[j].kategori)) continue;
            if (um_esit(dizi[i].kaynak, dizi[j].kaynak)) continue;
            /* AYNI özgün belgenin ayrı parçaları çatışma değildir. */
            char ka[96], kb[96];
            kaynak_koku(dizi[i].kaynak, ka, sizeof ka);
            kaynak_koku(dizi[j].kaynak, kb, sizeof kb);
            if (ka[0] && um_esit(ka, kb)) continue;
            if (dizi[i].skor - dizi[j].skor > 1) continue;   /* biri açıkça baskınsa çatışma sayılmaz */
            if (!c) return 1;
            c->var = 1;
            um_kopya(c->kategori, sizeof c->kategori, dizi[i].kategori);
            um_kopya(c->kaynak_a, sizeof c->kaynak_a, dizi[i].kaynak);
            um_kopya(c->kaynak_b, sizeof c->kaynak_b, dizi[j].kaynak);
            um_kopya(c->baslik_a, sizeof c->baslik_a, dizi[i].baslik);
            um_kopya(c->baslik_b, sizeof c->baslik_b, dizi[j].baslik);
            return 1;
        }
    return 0;
}

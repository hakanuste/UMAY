/* =====================================================================
 * UMAY — ana bilgisayar giriş noktası (komut satırı kabuğu)
 *
 * NEDEN AYRI DOSYA: çekirdek (core/) platform bağımsızdır; ÖTÜKEN'de
 * stdio yoktur. main() bir süre çekirdeğin içinde durdu ve orada
 * printf kullandı — yanlıştı. Giriş noktası artık burada; ÖTÜKEN'de
 * ise port/umay_port_otuken.c içindeki _start'tır. Çekirdek ikisini
 * de tanımaz, yalnız yaşam döngüsü çağrılarını bilir.
 *
 * İki kip:
 *   umay                konuşma kipi — uçbirimde sürekli
 *   umay "soru"         tek soru — yanıtlar ve çıkar (betikler için)
 * ===================================================================== */
#include <stdio.h>

#include "umay.h"
#include "umay_kabuk.h"
#include "umay_str.h"
#include "umay_gui.h"
#include "umay_port.h"
#include "umay_ogren.h"
#include "umay_arastir.h"
#include "umay_kabul.h"
#include "umay_ozturk.h"
#include "umay_ortak.h"
#include "umay_egit.h"
#include "umay_bilgi.h"

static void kullanim(const char *ad)
{
    printf("UMAY %s — Türkçe konuşan, çevrim dışı yardımcı\n\n", UMAY_SURUM);
    printf("Kullanım:\n");
    printf("  %s                konuşma kipi (uçbirim)\n", ad);
    printf("  %s \"soru\"         tek soru sorar, yanıtlar, çıkar\n", ad);
    printf("  %s --ogren        bekleyen boşlukları web'den öğrenir\n", ad);
    printf("  %s --ogren \"konu\" bir konuyu web'den araştırıp kütüphaneye ekler\n", ad);
    printf("  %s --arastir \"soru\" planlı, kaynaklı araştırma raporu\n", ad);
    printf("  %s --egit         bir eğitim turu (ölç, öğren, kapıdan geçir)\n", ad);
    printf("  %s --kabul         gelen/ içindeki belgeleri ajanlar inceleyip alanına koyar\n", ad);
    printf("  %s --5n1k          uzmanların 5N1K kapsam denetimi\n", ad);
    printf("  %s --karne        ajanların başarı karnesi\n", ad);
    printf("  %s --surum        sürümü yazar\n", ad);
    printf("  %s --yardim       bu yazı\n\n", ad);
    printf("Ortam değişkenleri:\n");
    printf("  UMAY_KUT          kütüphane kökü\n");
    printf("  UMAY_OGREN=0      web araştırmasını ve boşluk öğrenmeyi kapatır\n");
    printf("  UMAY_PIPER        piper çalıştırılabiliri\n");
    printf("  UMAY_PIPER_SES    .onnx ses modeli\n");
    printf("  UMAY_SES_OYNAT    ses çalıcı komutu\n\n");
    printf("Kütüphane sırayla aranır: UMAY_KUT, ./kut, kurulum yeri,\n");
    printf("~/.local/share/umay/kut, /usr/local/share/umay/kut\n");
}


/* ---- --veri: ince ayar için sohbet biçiminde JSONL (dayanaklı yanıt, "bulamadım", öz Türkçe) ---- */
static void json_yaz(const char *s) {
    putchar('"');
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"') fputs("\\\"", stdout);
        else if (c == '\\') fputs("\\\\", stdout);
        else if (c == '\n' || c == '\r' || c == '\t') fputs("\\n", stdout);
        else if (c < 0x20) putchar(' ');
        else putchar(c);
    }
    putchar('"');
}
static void veri_satir(const char *dayanak, const char *soru, const char *yanit) {
    fputs("{\"messages\":[{\"role\":\"system\",\"content\":", stdout);
    json_yaz("Sen UMAY'sın. Yalnız 'DAYANAK' bölümündeki bilgiye dayanarak, öz Türkçe, kısa ve açık yanıtla. Dayanakta olmayanı uydurma; yetmiyorsa 'bunu dayanağımda bulamadım' de.");
    fputs("},{\"role\":\"user\",\"content\":", stdout);
    static char u[4096];
    um_bicim(u, sizeof u, "DAYANAK:\n%s\n\nSORU: %s", dayanak, soru);
    json_yaz(u);
    fputs("},{\"role\":\"assistant\",\"content\":", stdout);
    json_yaz(yanit);
    fputs("}]}\n", stdout);
}
static int veri_uret(void) {
    static const struct { soru_tipi_t t; const char *kalip; } SK[] = {
        { ST_NE, "%s nedir?" }, { ST_NASIL, "%s nasıl yapılır?" }, { ST_NEDEN, "%s neden önemli?" },
        { ST_NEREDE, "%s nerede kullanılır?" }, { ST_KAC, "%s kaç derece, kaç ölçü?" },
    };
    int n = bilgi_sayi(), yazilan = 0, atilan_yabanci = 0, olumsuz = 0, atla = 0;
    static char dayanak[1500], soru[160], yanit[900], bas[64];
    for (int i = 0; i < n; i++) {
        bilgi_ham_t k;
        if (bilgi_kayit_oku(i, &k) != UM_OLDU) continue;
        if (k.kaynak_tur == KT_DIS || !um_esit(k.dil, "tr") || um_uzun(k.icerik) < 120) { atla++; continue; }
        um_kopya(dayanak, sizeof dayanak, k.icerik);
        um_kopya(bas, sizeof bas, k.baslik);
        for (unsigned q = 0; q < sizeof SK / sizeof SK[0]; q++) {
            if (!bilgi_tip_ipucu_var(SK[q].t, dayanak)) continue;
            um_bicim(soru, sizeof soru, SK[q].kalip, bas);
            /* yanıt: ipucu taşıyan ilk üç cümle (dayanaktan aynen) */
            yanit[0] = 0; int say = 0;
            const char *p = dayanak;
            while (*p && say < 3) {
                char cum[400]; uint32_t o = 0;
                while (*p && o + 1 < sizeof cum) { cum[o++] = *p; if (*p == '.' && (p[1] == ' ' || p[1] == '\n' || !p[1])) { p++; break; } p++; }
                cum[o] = 0;
                if (o > 20 && bilgi_tip_ipucu_var(SK[q].t, cum)) { if (yanit[0]) um_ekle(yanit, sizeof yanit, " "); um_ekle(yanit, sizeof yanit, cum); say++; }
            }
            if (!yanit[0]) continue;
            ozturk_bulgu_t b[2];
            if (ozturk_tara(yanit, b, 2) > 0) { atilan_yabanci++; continue; }   /* yabancı sözcük öğretme */
            veri_satir(dayanak, soru, yanit); yazilan++;
        }
        /* olumsuz örnek: başka kaydın başlığı sorulur, dayanak yetmez */
        bilgi_ham_t d;
        if (bilgi_kayit_oku((i * 7 + 3) % n, &d) == UM_OLDU && !um_esit(d.kategori, k.kategori) && d.kaynak_tur != KT_DIS) {
            um_bicim(soru, sizeof soru, "%s nedir?", d.baslik);
            veri_satir(dayanak, soru, "Bunu dayanağımda bulamadım."); olumsuz++;
        }
    }
    fprintf(stderr, "veri: %d örnek (%d olumsuz) · yabancı sözcük yüzünden atılan %d · atlanan kayıt %d\n",
            yazilan + olumsuz, olumsuz, atilan_yabanci, atla);
    return yazilan + olumsuz;
}

int main(int argc, char **argv)
{
    const char *ad = (argc > 0 && argv[0]) ? argv[0] : "umay";

    if (argc > 1) {
        if (um_esit(argv[1], "--yardim") || um_esit(argv[1], "-y") ||
            um_esit(argv[1], "--help")   || um_esit(argv[1], "-h")) {
            kullanim(ad);
            return 0;
        }
        if (um_esit(argv[1], "--surum")   || um_esit(argv[1], "-s") ||
            um_esit(argv[1], "--version") || um_esit(argv[1], "-V")) {
            printf("UMAY %s\n", UMAY_SURUM);
            return 0;
        }
        if (um_esit(argv[1], "--kabul")) {
            gui_sade_kip();
            if (umay_kur() != 0) { fprintf(stderr, "UMAY: baslatilamadi\n"); return 1; }
            gui_girdi_kapat();
            gui_sade_coz();
            static char ck[8192];
            kabul_sonuc_t ks;
            kabul_calistir(ayar_metin("kutuphane", "kut"), &ks, ck, sizeof ck);
            fputs(ck, stdout);
            umay_kapan();
            return 0;
        }
        if (um_esit(argv[1], "--veri")) {
            gui_sade_kip();
            if (umay_kur() != 0) { fprintf(stderr, "UMAY: baslatilamadi\n"); return 1; }
            gui_girdi_kapat();
            ozturk_yukle(ayar_metin("kutuphane", "kut"));
            int v = veri_uret();
            umay_kapan();
            return v > 0 ? 0 : 1;
        }
        if (um_esit(argv[1], "--5n1k")) {
            gui_sade_kip();
            if (umay_kur() != 0) { fprintf(stderr, "UMAY: baslatilamadi\n"); return 1; }
            gui_girdi_kapat();
            gui_sade_coz();
            static char c5[4096];
            egit_5n1k(1, c5, sizeof c5);
            fputs(c5, stdout);
            umay_kapan();
            return 0;
        }
        if (um_esit(argv[1], "--arastir") || um_esit(argv[1], "--egit") || um_esit(argv[1], "--karne")) {
            int arastir = um_esit(argv[1], "--arastir"), egit = um_esit(argv[1], "--egit");
            if (arastir && argc < 3) { fprintf(stderr, "Kullanım: %s --arastir \"soru\"\n", ad); return 2; }
            gui_sade_kip();
            if (umay_kur() != 0) { fprintf(stderr, "UMAY: baslatilamadi\n"); return 1; }
            gui_girdi_kapat();
            gui_sade_coz();
            static char cikti[4096];
            if (arastir) {
                char soru[UMAY_ILETI_MAX]; soru[0] = 0;
                for (int i = 2; i < argc; i++) { if (i > 2) um_ekle(soru, sizeof soru, " "); um_ekle(soru, sizeof soru, argv[i]); }
                static arastirma_t r;
                arastir_yurut(soru, "ortak", 1, &r);
                arastir_rapor(&r, cikti, sizeof cikti);
                karne_yaz("ARAŞTIRMACI", soru, (int)r.guven, r.kaynak_sayi);
            } else if (egit) {
                egit_sonuc_t sn;
                if (egit_calistir(5, &sn) == UM_OLDU) egit_rapor(&sn, cikti, sizeof cikti);
                else um_bicim(cikti, sizeof cikti, "Eğitim turu yapılamadı: %s\n", sn.neden);
            } else {
                karne_ozet(cikti, sizeof cikti);
            }
            fputs(cikti, stdout);
            umay_kapan();
            return 0;
        }
        if (um_esit(argv[1], "--ogren") || um_esit(argv[1], "--öğren")) {
            gui_sade_kip();
            if (umay_kur() != 0) { fprintf(stderr, "UMAY: baslatilamadi\n"); return 1; }
            gui_girdi_kapat();
            gui_sade_coz();
            if (!ogren_acik()) {
                printf("Öğrenme kapalı ya da web'e çıkış yolu yok (curl bulunamadı ya da UMAY_OGREN=0).\n");
                umay_kapan();
                return 3;
            }
            int toplam;
            if (argc > 2) {
                char konu[UMAY_ILETI_MAX]; konu[0] = 0;
                for (int i = 2; i < argc; i++) { if (i > 2) um_ekle(konu, sizeof konu, " "); um_ekle(konu, sizeof konu, argv[i]); }
                toplam = ogren_arastir("ortak", konu);
                if (toplam > 0) printf("'%s' için %d parça öğrenildi (kütüphaneye yazıldı, doğrulanmamış web kaynağı).\n", konu, toplam);
                else if (toplam == 0) printf("'%s' için yeni bir şey bulunamadı (ya da zaten öğrenilmişti).\n", konu);
                else printf("Ağa ulaşılamadı; konu sonra denenmek üzere bekletildi.\n");
            } else {
                int bekleyen = ogren_bekleyen();
                if (bekleyen == 0) { printf("Bekleyen boşluk yok.\n"); umay_kapan(); return 0; }
                toplam = ogren_gecis(bekleyen > 25 ? 25 : bekleyen);
                if (toplam > 0) printf("%d bekleyen boşluktan toplam %d parça öğrenildi; %d boşluk kaldı.\n", bekleyen, toplam, ogren_bekleyen());
                else if (toplam == 0) printf("Öğrenilecek yeni bir şey yok (bekleyen boşluk: %d).\n", ogren_bekleyen());
                else printf("Ağa ulaşılamadı; %d boşluk bekliyor.\n", bekleyen);
            }
            umay_kapan();
            return toplam >= 0 ? 0 : 3;
        }
        if (argv[1][0] == '-') {
            fprintf(stderr, "UMAY: bilinmeyen seçenek: %s\n", argv[1]);
            fprintf(stderr, "  %s --yardim\n", ad);
            return 2;
        }

        /* --- tek soru kipi --- */
        char soru[UMAY_ILETI_MAX];
        soru[0] = 0;
        for (int i = 1; i < argc; i++) {
            if (i > 1) um_ekle(soru, sizeof soru, " ");
            um_ekle(soru, sizeof soru, argv[i]);
        }
        /* SADE KIP, umay_kur()'DAN ONCE: levha orada basilir.
         * Bu kip "betikler icin" diye tanitiliyor, oysa cikti levha,
         * selam, durum satirlari ve "— ocak sondu —" ile geliyordu;
         * cagiran hepsini suzmek zorundaydi. Artik cikti yalniz
         * yanittir. Konusma kipi suslemesini korur. */
        gui_sade_kip();
        if (umay_kur() != 0) {
            fprintf(stderr, "UMAY: baslatilamadi\n");
            return 1;
        }
        /* Bir soru sorup cikacagiz: ucbirim girdisiyle isimiz yok.
         * Yoklasaydik cagiranin stdin'ini BOSALTIRDIK (olculdu). */
        gui_girdi_kapat();
        gui_sade_coz();        /* kurulus selami gecti; yanit basilabilir */
        umay_sor(soru);
        /* Ajanlar koşut çalışır; yanıt gelene dek dönülür. Üst sınır
         * var: bir ajan takılırsa program sonsuza dek beklemesin. */
        for (int t = 0; t < 600 && umay_mesgul(); t++) {
            umay_tur();
            up_uyu(16);
        }
        umay_tur();
        umay_kapan();
        return 0;
    }

    if (umay_kur() != 0) {
        fprintf(stderr, "UMAY: baslatilamadi\n");
        return 1;
    }
    while (umay_kosuyor()) {
        umay_tur();
        up_uyu(16);            /* ~60 tur/sn; boşta işlemci yakmaz */
    }
    umay_kapan();
    return 0;
}

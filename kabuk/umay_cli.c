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

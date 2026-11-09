/* Gomulu kitaplik sinamasi: uygulama gibi baglanir (umay_cli.c YOK). */
#include <stdio.h>
#include <string.h>
#include "umay_gomulu.h"

static int gecen, kalan;
static void dene(const char *ad, int ok) { printf("  [%s] %s\n", ok ? "GECTI" : "KALDI", ad); if (ok) gecen++; else kalan++; }

int main(int argc, char **argv)
{
    char c[4096];
    dene("surum dizgesi var", umay_gomulu_surum() && umay_gomulu_surum()[0]);
    dene("baslatmadan sor reddedilir", umay_gomulu_sor("merhaba", c, sizeof c) < 0);
    dene("baslat", umay_gomulu_baslat(argc > 1 ? argv[1] : 0) == 0);

    int n = umay_gomulu_sor("merhaba", c, sizeof c);
    dene("selam yanit verir", n > 0 && (int)strlen(c) == n);

    n = umay_gomulu_sor("güneş paneli nedir", c, sizeof c);
    dene("alan sorusu yanit verir", n > 0);
    dene("yanit dayanak gosterir", strstr(c, "Dayan") != 0);
    dene("yanit UTF-8 Turkce", strstr(c, "güneş") != 0 || strstr(c, "Güneş") != 0 || strstr(c, "KÜN") != 0);

    char k[16];
    n = umay_gomulu_sor("güneş paneli nedir", k, sizeof k);
    dene("kucuk tampon kirpilir, tasmaz", n >= 0 && n < (int)sizeof k && (int)strlen(k) == n);

    dene("bos soru reddedilir", umay_gomulu_sor("", c, sizeof c) < 0);
    n = umay_gomulu_sor("ikinci soru: kompost nedir", c, sizeof c);
    dene("art arda soru calisir", n > 0);

    /* ---- AJANLAR: CANLI DURUM ----
     * Bu bilgi kopruden GECMIYORDU: uygulamanin "Ajanlar" panosu bir
     * tanitim listesiydi, hangi ajanin kostugunu gosteremiyordu. */
    {
        char ad[32], is[96], alan[32], turler[128], gorev[192];
        int durum = -1, ilerleme = -1; uint32_t gecen = 0;

        int say = umay_gomulu_ajan_say();
        dene("ajan sayisi 18", say == 18);

        int bulunan = 0, uzman = 0, genel = 0, turu_olan = 0;
        for (int i = 0; i < say; i++) {
            if (umay_gomulu_ajan_bilgi(i, ad, sizeof ad, is, sizeof is,
                                       alan, sizeof alan, turler, sizeof turler,
                                       gorev, sizeof gorev,
                                       &durum, &ilerleme, &gecen) != 0) continue;
            bulunan++;
            if (alan[0]) uzman++; else genel++;
            if (turler[0]) turu_olan++;
        }
        dene("her ajanin bilgisi okunur", bulunan == say);
        dene("12 uzman, 6 genel gorevli", uzman == 12 && genel == 6);
        dene("her ajanin gorev turu bildirilir", turu_olan == say);

        /* Bilinen bir ajan dogru alanlarla gelmeli. */
        int tulga = 0;
        for (int i = 0; i < say; i++) {
            if (umay_gomulu_ajan_bilgi(i, ad, sizeof ad, is, sizeof is,
                                       alan, sizeof alan, turler, sizeof turler,
                                       gorev, sizeof gorev,
                                       &durum, &ilerleme, &gecen) != 0) continue;
            if (strcmp(ad, "TULGA") == 0) {
                /* alan KUCUK HARFLI ANAHTAR olmali: uygulamanin amblem
                 * eslemesi ve kut/ dizini onunla calisiyor. Ilk surumde
                 * alan_adi() dondurulmustu ve "TULGA" geliyordu — o,
                 * 'ad' ile ayni sey, yani yineleme. */
                tulga = (strstr(is, "otomotiv") != 0) && (strcmp(alan, "tulga") == 0);
            }
        }
        dene("TULGA adi, isi ve alani dogru", tulga);

        /* Is yokken canli alanlar BOS olmali: bitmis bir ajanin eski
         * gorevini gostermek, calistigini sanmaya yol acar. */
        int canli_kirli = 0;
        for (int i = 0; i < say; i++) {
            if (umay_gomulu_ajan_bilgi(i, ad, sizeof ad, is, sizeof is,
                                       alan, sizeof alan, turler, sizeof turler,
                                       gorev, sizeof gorev,
                                       &durum, &ilerleme, &gecen) != 0) continue;
            if (durum != 1 && durum != 2 && (gorev[0] || ilerleme || gecen)) canli_kirli = 1;
        }
        dene("kosmayan ajanda gorev/ilerleme/sure bos", !canli_kirli);

        /* Sinir ve gecersiz girdi */
        dene("gecersiz sira reddedilir",
             umay_gomulu_ajan_bilgi(-1, ad, sizeof ad, is, sizeof is, alan, sizeof alan,
                                    turler, sizeof turler, gorev, sizeof gorev,
                                    &durum, &ilerleme, &gecen) != 0 &&
             umay_gomulu_ajan_bilgi(say, ad, sizeof ad, is, sizeof is, alan, sizeof alan,
                                    turler, sizeof turler, gorev, sizeof gorev,
                                    &durum, &ilerleme, &gecen) != 0);
        char kucuk[4];
        umay_gomulu_ajan_bilgi(0, kucuk, sizeof kucuk, is, sizeof is, alan, sizeof alan,
                               turler, sizeof turler, gorev, sizeof gorev,
                               &durum, &ilerleme, &gecen);
        dene("kucuk tampon tasmaz ve sonlanir", strlen(kucuk) < sizeof kucuk);
        dene("imlecler bos verilebilir",
             umay_gomulu_ajan_bilgi(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) == 0);
    }

    umay_gomulu_kapat();
    dene("kapattiktan sonra sor reddedilir", umay_gomulu_sor("merhaba", c, sizeof c) < 0);
    printf("=== GOMULU SONUC: %d gecti, %d kaldi ===\n", gecen, kalan);
    return kalan ? 1 : 0;
}

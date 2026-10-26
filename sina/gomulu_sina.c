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

    umay_gomulu_kapat();
    dene("kapattiktan sonra sor reddedilir", umay_gomulu_sor("merhaba", c, sizeof c) < 0);
    printf("=== GOMULU SONUC: %d gecti, %d kaldi ===\n", gecen, kalan);
    return kalan ? 1 : 0;
}

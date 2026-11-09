/* PDF AYRISTIRICI YAGMURU — "make pdf-yagmur"
 *
 * NEDEN: PDF ayristirici GUVENILMEZ girdi okur (kullanicinin
 * yukledigi, agdan inen belgeler) ve 1065 satir, 89 donguden olusur.
 * Ayristiricilar bellek guvenliginin klasik kirilma yeridir. sina
 * takiminda PDF icin yalniz 6 denetim vardi; bir ayristirici icin bu
 * ince kalir — 6 iddia, 89 dongunun davranisini kapsamaz.
 *
 * NE YAPAR: gercekci bir PDF iskeletinin bozulmus, kesilmis ve asiri
 * ic ice surumlerini ASan + UBSan altinda ayristiriciya verir. Cikis
 * 0 = hic cokme, tasma ya da tanimsiz davranis yok.
 *
 * DERINLIK KANITI ZORUNLU: bastan reddedilen girdi yigini hicbir sey
 * kanitlamaz. Bu yuzden donus dagilimi sayilir ve ayristirici hic
 * sonuna kadar kosmadiysa ya da hic metin uretmediyse cikis 2 verir:
 * "gecti" demek yerine "yagmur derine inmedi" der.
 *
 * OLCULEN (ilk kosu): 121038 cagri, cokme YOK; 13175 cagri
 * ayristiricinin sonuna kadar kostu ve 13039'u gercekten metin
 * uretti, yani icerik cozme koduna inildi. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "umay_pdf.h"

static uint32_t tohum = 12345;
static uint32_t rnd(void){ tohum = tohum*1103515245u + 12345u; return (tohum>>8); }

/* Gercekci bir PDF iskeleti: ayristirici derine insin. */
static const char ISKELET[] =
"%PDF-1.4\n"
"1 0 obj << /Type /Catalog /Pages 2 0 R >> endobj\n"
"2 0 obj << /Type /Pages /Kids [3 0 R] /Count 1 >> endobj\n"
"3 0 obj << /Type /Page /Parent 2 0 R /Contents 4 0 R >> endobj\n"
"4 0 obj << /Length 44 >> stream\n"
"BT /F1 12 Tf 72 712 Td (merhaba dunya) Tj ET\n"
"endstream endobj\n"
"5 0 obj << /Type /Font /Subtype /Type1 /BaseFont /Helvetica >> endobj\n"
"trailer << /Root 1 0 R >>\n"
"startxref\n0\n%%EOF\n";

int main(void){
    char cik[1<<16];
    int sayfa = 0;
    long cagri = 0;
    /* DONUS DAGILIMI: yagmurun derine indigini KANITLAMAK icin.
     * Hepsi "PDF degil" donuyorsa ayristirici hic kosmamis demektir. */
    long kod[8] = {0};
    long inf_ok = 0, inf_hata = 0, metin_cikti = 0;
#define SAY(r) do { int _r=(r); if (_r>=0) kod[0]++; else if (_r==PDF_BOZUK) kod[1]++; \
    else if (_r==PDF_SIFRELI) kod[2]++; else kod[3]++; } while (0)

    /* 1) Iskeletin rastgele BOZULMUS surumleri */
    for (int tur = 0; tur < 40000; tur++) {
        uint32_t n = (uint32_t)sizeof ISKELET - 1;
        uint8_t *b = (uint8_t*)malloc(n + 1);
        memcpy(b, ISKELET, n);
        int kac = 1 + (int)(rnd() % 12);
        for (int i = 0; i < kac; i++) b[rnd() % n] = (uint8_t)(rnd() & 0xFF);
        (void)pdf_mi(b, n);
        cik[0]=0; SAY(pdf_metin(b, n, cik, sizeof cik, &sayfa));
        if (cik[0]) metin_cikti++;
        free(b); cagri += 2;
    }

    /* 2) KISALTILMIS girdiler: her uzunlukta kesilmis PDF */
    for (uint32_t n = 0; n < (uint32_t)sizeof ISKELET; n++) {
        (void)pdf_mi((const uint8_t*)ISKELET, n);
        SAY(pdf_metin((const uint8_t*)ISKELET, n, cik, sizeof cik, &sayfa));
        cagri += 2;
    }

    /* 3) KUCUK cikti arabellegi: tasma olur mu */
    for (uint32_t cn = 1; cn <= 64; cn++) {
        (void)pdf_metin((const uint8_t*)ISKELET, (uint32_t)sizeof ISKELET - 1, cik, cn, &sayfa);
        cagri++;
    }

    /* 4) inflate: bozuk ve kesilmis akislar */
    for (int tur = 0; tur < 40000; tur++) {
        uint8_t g[160]; uint8_t o[4096];
        uint32_t n = 1 + (rnd() % sizeof g);
        g[0] = 0x78; if (n > 1) g[1] = (uint8_t)(rnd() & 0xFF);
        for (uint32_t i = 2; i < n; i++) g[i] = (uint8_t)(rnd() & 0xFF);
        uint32_t cn = 1 + (rnd() % sizeof o);
        if (pdf_inflate(g, n, o, cn) >= 0) inf_ok++; else inf_hata++;
        cagri++;
    }

    /* 5) Derin ic ice ve asiri uzun belirtecler */
    for (int tur = 0; tur < 200; tur++) {
        uint32_t n = 40000;
        uint8_t *b = (uint8_t*)malloc(n + 1);
        memcpy(b, "%PDF-1.4\n", 9);
        uint32_t o = 9;
        int kip = tur % 4;
        while (o < n - 2) {
            const char *p = kip==0 ? "<< /K " : kip==1 ? "[ " : kip==2 ? "(" : "1 0 obj << /Length 9 >> stream\n";
            uint32_t l = (uint32_t)strlen(p);
            if (o + l >= n - 2) break;
            memcpy(b + o, p, l); o += l;
        }
        b[o] = 0;
        (void)pdf_metin(b, o, cik, sizeof cik, &sayfa);
        free(b); cagri++;
    }

    printf("  %ld cagri yapildi, cokme/tasma yok.\n", cagri);
    printf("  DERINLIK KANITI — pdf_metin donusleri:\n");
    printf("    basarili (>=0)     : %ld  <- ayristirici sonuna kadar kostu\n", kod[0]);
    printf("    PDF_BOZUK          : %ld\n", kod[1]);
    printf("    PDF_SIFRELI        : %ld\n", kod[2]);
    printf("    oteki hata         : %ld\n", kod[3]);
    printf("    metin URETILEN cagri: %ld  <- icerik gercekten cozuldu\n", metin_cikti);
    printf("  inflate: %ld basarili, %ld reddedildi\n", inf_ok, inf_hata);
    if (kod[0] == 0 || metin_cikti == 0) {
        printf("  >>> YAGMUR DERINE INMEDI: bu sonuc bir sey KANITLAMAZ <<<\n");
        return 2;
    }
    return 0;
}

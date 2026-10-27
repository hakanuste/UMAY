/* =====================================================================
 * PDF — PDF belgesinden düz metin çıkarma (dış kütüphanesiz).
 *
 * Kapsam: FlateDecode akışları (kendi inflate'imiz), nesne akışları
 * (ObjStm, PDF 1.5+), sayfa ağacı sırası, Tj/TJ/'/" metin işleçleri,
 * ToUnicode (bfchar/bfrange) ile Type0/TrueType yazı türleri, tek baytlık
 * yazı türleri için CP1252. Tamsayı hesap; ÖTÜKEN'de de derlenir.
 *
 * Yapamadıkları AÇIKÇA bildirilir (sessiz boş sonuç yok):
 *   PDF_SIFRELI   — şifreli PDF
 *   PDF_METINSIZ  — metin katmanı yok (taranmış görüntü; OCR gerekir)
 *   PDF_BOZUK     — nesne yapısı okunamadı
 * ===================================================================== */
#ifndef UMAY_PDF_H
#define UMAY_PDF_H
#include "umay.h"

#define PDF_SIFRELI  (-2)
#define PDF_METINSIZ (-3)
#define PDF_BOZUK    (-4)

/* PDF baytlarından metin çıkarır; paragraflar boş satırla ayrılır.
 * Dönüş: yazılan bayt sayısı (>0) ya da yukarıdaki hata kodlarından biri.
 * sayfa_sayi (boş olabilir): çıkarılan sayfa sayısı. */
int pdf_metin(const uint8_t *pdf, uint32_t n, char *cik, uint32_t cn, int *sayfa_sayi);
/* Dosya PDF mi? (başlıkta %PDF-) */
int pdf_mi(const uint8_t *b, uint32_t n);
/* Akış çözücü (sınama için açık): Flate/zlib verisini açar, bayt sayısını döner (<0 hata). */
int pdf_inflate(const uint8_t *girdi, uint32_t n, uint8_t *cik, uint32_t cn);
#endif

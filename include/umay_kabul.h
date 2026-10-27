/* =====================================================================
 * KABUL — kullanıcının getirdiği kaynakların ajanlarca incelenip
 * gerektiği yere konması.
 *
 * Kullanıcı belgelerini <kütüphane>/gelen/ dizinine atar (.txt/.md, künyeli
 * ya da künyesiz). YAZIT ajanı (kabul_calistir) her belgeyi şöyle işler:
 *   1. MÜHÜR    belgenin özeti (FNV-1a) işlenenler listesine bakılır; aynı
 *               belge iki kez alınmaz.
 *   2. KÜNYE    künyesi geçerliyse (#kategori) olduğu gibi o alana konur.
 *   3. İNCELEME künyesizse belge ~2400 baytlık parçalara bölünür; her parça
 *               12 uzmanın iz sözcükleriyle puanlanır. Parçanın kendi puanı
 *               zayıfsa belgenin geneline, o da yoksa bilgi tabanındaki en
 *               yakın kayda bakılır.
 *   4. YERLEŞİM parça, kazanan uzmanın alan dizinine künyeli olarak yazılır.
 *               İkinci alan yakın puandaysa parça ona da konur (çok alanlı).
 *               Kimseye varamayan parça ORTAK'a konur ve raporda "belirsiz"
 *               diye işaretlenir; sessizce uydurma sınıflama yapılmaz.
 *   5. RAPOR    hangi belge hangi alana kaç parça gitti, ne belirsiz kaldı.
 *
 * Kaynak türü KT_BELGE'dir (kullanıcı eliyle getirilmiş, doğrulanmış sayılır);
 * web'den öğrenilenler (KT_DIS) gibi güvensiz değildir. Model çağrılmaz;
 * belirlenimlidir. Tamsayı hesap, öbek tamponlar: ÖTÜKEN'de de derlenir.
 * ===================================================================== */
#ifndef UMAY_KABUL_H
#define UMAY_KABUL_H
#include "umay.h"

typedef struct {
    int dosya;        /* bu turda işlenen belge */
    int parca;        /* yazılan parça (çok alanlı kopyalar dahil) */
    int belirsiz;     /* ortak'a düşen parça */
    int tekrar;       /* zaten işlenmiş / var olan, atlanan */
    int desteklenmeyen; /* metin olmayan dosya */
    int yeni_kayit;   /* kütüphane indeksine eklenen kayıt */
} kabul_sonuc_t;

/* gelen/ dizinini yaratır (yoksa) ve içindeki yeni belgeleri işler.
 * rapor: okunur özet. Dönüş: yazılan parça sayısı. */
int  kabul_calistir(const char *kok, kabul_sonuc_t *s, char *rapor, uint32_t n);
/* gelen/ içinde henüz işlenmemiş dosya sayısı (ucuz yoklama). */
int  kabul_bekleyen(const char *kok);
/* Boştayken (umay_tur): bekleyen varsa bir tur kabul eder. */
void kabul_bosta(void);
#endif

/* =====================================================================
 * EĞİT — ajan karnesi ve KAPILI eğitim.
 *
 * KARNE: her ajanın her yanıtı (hangi ajan, hangi soru, güven, kaç
 * dayanak) yerel dosyaya yazılır: <kütüphane>/_ogren/karne.txt.
 * Yalnız cihazda kalır.
 *
 * EĞİTİM bir döngüdür, her turda:
 *   1. ÖLÇ      karnedeki son sorular kütüphanede ne kadar karşılanıyor
 *               (web'siz, tamsayı kapsam, binde).
 *   2. ÖĞREN    zayıf sorulardan çıkan boşlukları web'den kapat.
 *   3. YENİDEN ÖLÇ.
 *   4. KAPI     kazanç en az %2 (20/1000) olmalı VE önceden karşılanan
 *               soruların en az %85'i hâlâ karşılanmalı. Yoksa tur
 *               GERİ ALINIR: eklenen kayıtlar dizinden atılır, yazılan
 *               web dosyaları silinir.
 *
 * Dil modelinin ağırlıkları değişmez; öğrenilen şey kütüphanedir.
 * ===================================================================== */
#ifndef UMAY_EGIT_H
#define UMAY_EGIT_H
#include "umay.h"

#define KARNE_SORU_MAX 200

/* Bir yanıtı karneye yazar. guven: us_guven_t sayısı (0..3). */
void karne_yaz(const char *ajan, const char *soru, int guven, int dayanak);

/* Ajan başına özet: yanıt sayısı, ortalama güven (%), dayanaksız yanıt sayısı. */
uint32_t karne_ozet(char *c, uint32_t n);

typedef struct {
    int  soru_sayi;           /* değerlendirme kümesi */
    int  once, sonra;         /* ortalama kapsam, binde */
    int  korunan_once;        /* önceden karşılanan (>=50) soru sayısı */
    int  korunan_sonra;       /* bunların hâlâ karşılananı */
    int  eklenen;             /* eklenen parça */
    int  kabul;               /* 1 = tur kabul edildi */
    int  geri_alindi;         /* 1 = tur geri alındı */
    int  bekleyen;            /* tur sonunda bekleyen boşluk */
    char neden[128];
} egit_sonuc_t;

/* Bir eğitim turu. en_cok_bosluk: bu turda işlenecek boşluk. UM_OLDU / UM_YOK (kapalı/ağ yok). */
int  egit_calistir(int en_cok_bosluk, egit_sonuc_t *s);
uint32_t egit_rapor(const egit_sonuc_t *s, char *c, uint32_t n);

/* Boştayken seyrek aralıkla bir tur çalıştırır (umay_tur çağırır). */
void egit_bosta(void);

/* 5N1K denetimi: her uzman (12 alan) × altı soru türü (ne, nerede, ne zaman,
 * neden, nasıl, kim). Bir hücre, alanın konu başlıklarından en az biri için
 * soru türünün ipucunu taşıyan, kaynaklı bir yanıt bulunursa doludur.
 * Boş hücreler web-öğrenme boşluğu olarak kaydedilir (kayit=1). Uydurma yok:
 * boşluk, kaynakla (web ya da kullanıcı belgesi) kapanır.
 * Dönüş: boş hücre sayısı. */
int egit_5n1k(int kayit, char *cikti, uint32_t n);
#endif

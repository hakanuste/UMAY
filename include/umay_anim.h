/* ANIM: anımsatıcı ve ilaç alarmı. TAŞINABİLİR çekirdek (Linux, Windows, macOS, ÖTÜKEN).
 *
 * Çekirdek işletim sistemine dokunmaz: saati port'tan (up_yerel_dakika) alır, kaydı
 * kütüphanedeki animsa.txt kütüğüne yazar, bildirimi port'a (up_bildir) bırakır.
 * Böylece hangi bilgisayara geçilirse geçilsin aynı kod çalışır.
 *
 * İKİ TÜR:
 *   'O' olay    : bir kez (ya da her gün/hafta) bildirilir.
 *   'I' ilaç    : vakti gelince çalar; kullanıcı "içtim" diyene dek 5 dakikada bir yinelenir.
 *
 * GİZLİLİK: her şey yalnız yerel kütükte durur; ağa hiçbir şey gitmez.
 * ONAY: sohbetten gelen istek önce SORULUR ("Kaydedeyim mi?"); "evet" denmeden kayıt yazılmaz.
 *
 * ZAMAN: "yerel dakika" = 1970-01-01 00:00 yerel saatinden beri geçen dakika. Saat dilimi
 * kaydedilmez (kişisel alarm: duvar saati neyse odur). */
#ifndef UMAY_ANIM_H
#define UMAY_ANIM_H
#include "umay.h"

#define ANIM_MAX      48
#define ANIM_METIN    112
#define ANIM_ILAC_ARA 5      /* ilaç yinelemesi (dakika) */
#define ANIM_ILAC_AZ  12     /* en çok yineleme; sonra "kaçırıldı" sayılır */

typedef struct {
    int     dolu;
    int     id;
    char    tur;             /* 'O' ya da 'I' */
    int64_t vakit;           /* sıradaki çalış (yerel dakika) */
    int     tekrar;          /* 0 = bir kez; 1440 = her gün; 10080 = her hafta */
    int     durum;           /* 0 bekler, 1 çalıyor (ilaç, onay bekliyor) */
    int     calma;           /* kaç kez çaldı */
    int64_t zil;             /* çalıyorsa sıradaki yineleme (yerel dakika) */
    char    metin[ANIM_METIN];
} anim_t;

/* Saat kaynağı: varsayılan up_yerel_dakika. Sınama için değiştirilebilir. */
void    anim_saat_ata(int64_t (*f)(void));
void    anim_yol_ata(const char *yol);          /* kütük yolunu değiştirir (sınama); 0 = varsayılan */
int64_t anim_simdi(void);

/* Takvim yardımcıları (saf aritmetik) */
int64_t anim_dakika(int yil, int ay, int gun, int saat, int dakika);
void    anim_ayir(int64_t dk, int *yil, int *ay, int *gun, int *saat, int *dakika);
int     anim_haftagunu(int64_t dk);            /* 1=pazartesi ... 7=pazar */

/* Kayıt */
int     anim_ekle(char tur, int64_t vakit, int tekrar, const char *metin);   /* id ya da -1 */
int     anim_sil(int id);                                                    /* 1 = silindi */
int     anim_say(void);
const anim_t *anim_al(int i);                                                /* i. dolu kayıt */

/* Vakti gelenleri döner ve durumlarını ilerletir. Her satır gösterilecek bildirim metnidir.
 * satir[k] ANIM_METIN+48 bayt. Dönen: satır sayısı. */
#define ANIM_SATIR 176
int     anim_yokla(int64_t simdi, char satir[][ANIM_SATIR], int en_cok);

/* Çalan ilaçları onaylar ("içtim"); dönen: onaylanan sayısı. Sonraki gün aynı saate kurulur. */
int     anim_tamam(int64_t simdi);
/* Çalanları dk dakika sonraya erteler. */
int     anim_ertele(int64_t simdi, int dk);

/* Sohbet: cümle bir anımsatıcı isteğiyse yanıtı yazar ve 1 döner; değilse 0 (ajanlara gider). */
int     anim_konus(const char *soz, char *yanit, uint32_t n);

/* Okunur dizelge ("Anımsatıcılarım"). */
void    anim_dizelge(char *cik, uint32_t n);

/* Kütüğü yeniden okutur (başka bir süreç yazmış olabilir). Sınamada sıfırlamak için de kullanılır. */
void    anim_sifirla_bellek(void);
#endif

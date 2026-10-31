/* KASA: parolalar, sağlık ve arazi kütükleri gibi özel şeyler için ŞİFRELİ yerel kasa.
 *
 * ŞİFRELEME (kendimiz yazmadık; sınanmış Monocypher 4.0.2, third_party/monocypher):
 *   anahtar : Argon2id (64 MiB, 3 geçiş, 16 bayt tuz)  ← ana parola
 *   şifreleme: XChaCha20-Poly1305 (24 baytlık rastgele nonce, HER kayıtta yenisi)
 *   Başlık da doğrulanır: tek bayt değişse kasa açılmaz ("bozuk" der, yanlış veri vermez).
 *
 * KURALLAR
 *  - Ana parola sohbete YAZILMAZ. Sohbet, model ve KABUL kasaya hiç dokunmaz; kasa kütüğü
 *    (kasa.bin) kütüphanenin belge alanlarında değildir, dizine girmez.
 *  - Ağa hiçbir şey çıkmaz. Kasa içeriği günlüğe yazılmaz.
 *  - Ana parolayı unutursan içerik KURTARILAMAZ (arka kapı yoktur). Yedek al: kasa.bin + kasa.yed.
 *  - Açıkken 5 dakika boşta kalırsa kendiliğinden kilitlenir; anahtar ve düz metin bellekten silinir.
 *  - Bilgisayarda tuş kaydedici gibi bir zararlı varsa hiçbir kasa korumaz.
 *
 * Çekirdek taşınabilir C'dir; rastgele sayıyı port verir (up_rastgele). ÖTÜKEN'de rastgele
 * kaynağı yoktur: orada kasa_kur/kasa_ac -3 döner. */
#ifndef UMAY_KASA_H
#define UMAY_KASA_H
#include "umay.h"

#define KASA_AD_MAX   64
#define KASA_AZAMI    (8u << 20)      /* düz metin toplamı: 8 MiB */
#define KASA_PAROLA_AZ 10

#define KASA_METIN 0
#define KASA_DOSYA 1

/* dönüş kodları: 0 = oldu; -1 yok/var (işlevde açıklanır); -2 parola yanlış ya da kütük değiştirilmiş;
 * -3 bu aygıtta olmaz (rastgele kaynağı yok); -4 bellek/yer yetmedi; -5 parola çok kısa; -6 kilitli */
int      kasa_var_mi(void);                         /* kasa.bin var mı */
int      kasa_acik_mi(void);                        /* açık mı (boşta süresi dolduysa kilitler, 0 döner) */
int      kasa_kur(const char *parola);              /* yeni kasa; zaten varsa -1 */
int      kasa_ac(const char *parola);               /* kasa yoksa -1 */
void     kasa_kilitle(void);
int      kasa_say(void);                            /* kayıt sayısı; kilitliyse -6 */
int      kasa_bilgi(int i, char *ad, uint32_t n, int *tur, uint32_t *boy);   /* i. kayıt; yoksa -1 */
int      kasa_yaz(const char *ad, int tur, const void *veri, uint32_t n);    /* ekler ya da üstüne yazar */
int32_t  kasa_boy(const char *ad);                  /* kayıt boyu; yok -1; kilitli -6 */
int32_t  kasa_oku(const char *ad, void *cik, uint32_t n);   /* kopyalanan bayt; yok -1; tampon küçük -4 */
int      kasa_sil(const char *ad);                  /* 0 silindi, -1 yok */
/* Biyometrik kısayol (yüz/parmak izi): CANLILIK ve eşleşme işletim sisteminin donanımında doğrulanır
 * (Face ID, Android Strong biyometri, Windows Hello). UMAY kendi yüz tanımasını YAZMAZ: kamera görüntüsü
 * fotoğraf/video/derin sahtecilikle aldatılabilir. Ana parola her zaman yedek yoldur. */
int      kasa_anahtari_ver(uint8_t cik[32]);              /* yalnız açıkken; uygulama OS deposuna koyar */
int      kasa_ac_anahtarla(const uint8_t anahtar[32]);    /* OS biyometri onayından sonra */
int      kasa_parola_degistir(const char *eski, const char *yeni);

/* Sınama/ayar: */
void     kasa_yol_ata(const char *yol);             /* kasa.bin yolu; 0 = kütüphane/kasa.bin */
void     kasa_parametre_ata(uint32_t bloklar, uint32_t gecis);   /* yalnız sınama (hızlı Argon2) */
void     kasa_kilit_suresi_ata(uint32_t ms);
#endif

/* =====================================================================
 * MOTOR — gömülü dil modeli (LLM) sözleşmesi.
 *
 * Çekirdek C11'dir ve -Werror ile derlenir; llama.cpp ise C++17'dir. Bu
 * yüzden motor AYRI bir kütüphanedir (motor/), çekirdek ona yalnız bu küçük C
 * tablosu üzerinden bağlanır. umay_motor_api() iki yerde tanımlıdır; bir
 * yapıda yalnız biri bulunur (koşullu derleme yok, dosya seçimi var):
 *   us/umay_motor_yok.c      varsayılan: motor yok → NULL
 *   motor/umay_motor.cpp     gerçek: llama.cpp ile
 *
 * Dürüstlük: model yüklenemezse, bağlam taşarsa ya da çıktı boşsa motor
 * BAŞARISIZ döner; uydurma yanıt üretmez. Üst katman (us) stub'a düşer.
 * ===================================================================== */
#ifndef UMAY_MOTOR_H
#define UMAY_MOTOR_H
#include <stdint.h>

#define UMAY_MOTOR_TASTI   (-2)     /* istem + üretim bağlama sığmadı */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct umay_motor_api {
    /* GGUF modelini yükler. 0 = oldu. baglam: token sayısı (0 = varsayılan 2048),
     * is_parcacigi: 0 = otomatik. */
    int         (*yukle)(const char *gguf_yolu, int baglam, int is_parcacigi);
    /* Sohbet kalıbıyla (modelin kendi şablonu) yanıt üretir. 0 = oldu,
     * UMAY_MOTOR_TASTI = bağlam yetmedi (çağıran bağlamı kısaltıp yeniden dener),
     * diğer negatif = hata. */
    int         (*uret)(const char *sistem, const char *kullanici, uint32_t en_cok_im,
                        char *cikti, uint32_t n);
    void        (*kapat)(void);
    const char *(*ad)(void);          /* örn. "llama.cpp b0000 · model adı" */
} umay_motor_api_t;

/* Sağlayıcı: motor yoksa NULL. */
const umay_motor_api_t *umay_motor_api(void);

#ifdef __cplusplus
}
#endif
#endif

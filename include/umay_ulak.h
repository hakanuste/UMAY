/* ULAK canlı veri: kütüphanede bulunmayan, ANLIK değişen bilgiler.
 *   - Döviz kuru: TCMB günlük kur dosyası (today.xml)
 *   - Hava durumu: Open-Meteo (konum adı -> koordinat -> anlık durum)
 * Soru canlı veri sorusu değilse 0 döner ve hiçbir şey çekmez. Ağ yoksa ya da
 * "internetten öğren" kapalıysa da 0 döner: uydurulmuş sayı VERİLMEZ.
 * Çıktı, bilgi katmanının dayanak biçiminde (baglam_t) gelir; bu yüzden
 * kaynağı ve zamanı yanıtta anılır. Altın/borsa için güvenilir ücretsiz
 * kaynak seçilmedi: onlar için "canlı veri yok" denir. */
#ifndef UMAY_ULAK_H
#define UMAY_ULAK_H
#include "umay.h"
#include "umay_bilgi.h"

/* 1 = canlı dayanak üretildi, 0 = canlı veri sorusu değil / alınamadı. */
int ulak_canli(const char *soru, baglam_t *b);
#endif

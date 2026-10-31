/* SABAH: günlük bildiri. Kur (TCMB), altın/gümüş, Borsa İstanbul ve yabancı basından
 * (Almanya, Fransa, İngiltere, Japonya, Rusya…) başlıklar — Türkçeye çevrilmiş.
 *
 * DÜRÜSTLÜK
 *  - Hiçbir sayı uydurulmaz: bir kaynak alınamazsa o satıra "alınamadı" yazılır.
 *  - Haber başlıkları makine çevirisidir (doğrulanmamış); kaynağın türü (kamu / bağımsız) etikettedir.
 *  - "Piyasa beklentisi" için güvenilir ücretsiz bir kaynak yoktur: borsanın düzeyi ve günlük
 *    değişimi verilir, beklenti yerine yabancı basındaki Türkiye ekonomisi başlıkları konur.
 *  - Gram altın/gümüş, ons fiyatı × TCMB dolar satış / 31,1035 ile HESAPLANIR; kuyumcu fiyatı değildir.
 * GİZLİLİK
 *  - Ağa yalnız herkese açık adresler gider (kişisel hiçbir söz gönderilmez); çıkış, tek kapıdan
 *    (ogren_ag_getir: gizli sözcük duvarı + UMAY_OGREN anahtarı) geçer.
 * KAYNAKLAR
 *  - <kütüphane>/haberler.txt varsa onu, yoksa dahili listeyi kullanır. Örnek: docs/ornek/haberler.txt */
#ifndef UMAY_SABAH_H
#define UMAY_SABAH_H
#include "umay.h"

/* Bildiriyi cik'a yazar (UTF-8). Dönen: yazılan bayt. Ağ yoksa/kapalıysa nedeni söyleyen kısa metin. */
uint32_t sabah_bildirisi(char *cik, uint32_t n);

/* Cümle "sabah bildirisi / günün bildirisi / sabah raporu" isteği mi? */
int      sabah_sorusu(const char *soz);

/* Sınama için: kaynak ve portföy kütüğü yollarını değiştirir (0 = varsayılan). */
void     sabah_dosya_ata(const char *kaynaklar, const char *portfoy);

/* Dahili kaynak listesini döner (haberler.txt biçiminde). */
const char *sabah_varsayilan_kaynaklar(void);
#endif

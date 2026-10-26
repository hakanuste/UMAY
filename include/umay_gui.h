/* ÜLGEN arayüzü: konuşma ekranı, durum çubuğu, ajan/görev görünümü. */
#ifndef UMAY_GUI_H
#define UMAY_GUI_H
#include "umay.h"
#include "umay_alp.h"

#define GUI_SATIR_MAX 64
#define GUI_SUTUN_MAX 96

int  gui_ac(int en, int boy);
int  gui_acik_mi(void);
/* Ön yüz uçbirimi kendisi okuyup yazıyor mu? Konsol ön yüzünde 1'dir:
 * o durumda klavye kanalı KAYDEDİLMEZ, yoksa ikisi aynı stdin'den
 * okur ve girdi ikiye bölünür; yanıt da iki kez basılır. */
int  gui_konsolu_sahiplenir(void);
void gui_kapat(void);
/* Kullanıcı pencere kapatma düğmesine bastı mı? */
int  gui_kapat_istendi(void);
void gui_satir_ekle(const char *kim, const char *metin);   /* konuşma akışı */
void gui_durum(const char *metin);
void gui_ciz(const otag_t *o);
int  gui_giris_yokla(umay_ileti_t *i);                     /* 1 = yeni söz */

/* Ucbirim girdisini tumuyle kapatir (tek soru kipi). Cagrilmazsa
 * davranis degismez. */
/* SADE (BETIK) KIPI — "umay \"soru\"" icin.
 * gui_sade_kip() : levha, selam, durum satirlari ve veda basilmaz.
 *                  umay_kur()'DAN ONCE cagrilmali (levha orada basilir).
 * gui_sade_coz() : bundan sonraki satirlar basilir. Soruyu sormadan
 *                  hemen once cagrilir; boylece kurulus sirasindaki
 *                  selam ciktiya karismaz.
 * Ciktinin yalniz yanit olmasi betiklerin suzgec yazmasini onler. */
void gui_sade_kip(void);
void gui_sade_coz(void);
int  gui_sade_mi(void);

void gui_girdi_kapat(void);
/* Otağ görünümünü istek üzerine basar. Konsol ön yüzünde anlamlıdır;
 * ÜLGEN ön yüzü otağı zaten sürekli çizer, orada boştur. */
void gui_otag_yaz(const otag_t *o);
#endif

/* ZAMAN: yazıdan "ne zaman, ne yapılacak" ayıklar (hatırlatıcı/takvim için).
 *
 * "Pazartesi hortum değişimi için Kadosan'a gidilecek" ->  haftagunu=1, is="hortum değişimi için Kadosan'a gidilecek"
 * "Yarın saat 14:30 dişçi"                              ->  gun_ofset=1, saat=14, dakika=30, is="dişçi"
 *
 * Çıktı GÖRELİDİR (bugünün tarihi yoktur); kesin tarihe çevirmek uygulamanın işidir (Takvim/Hatırlatıcı
 * katmanı). Böylece çekirdek her yerde (Mac, iOS, Linux, ÖTÜKEN) aynen çalışır.
 * Hiçbir şey kendiliğinden kurulmaz: kullanıcı onaylamadan alarm/takvim yazılmaz. */
#ifndef UMAY_ZAMAN_H
#define UMAY_ZAMAN_H
#include "umay.h"

typedef struct {
    int var;          /* 1 = bir zaman ifadesi bulundu */
    int gun_ofset;    /* 0 bugün, 1 yarın, 2 öbür gün; -1 = belirtilmemiş */
    int haftagunu;    /* 1=pazartesi ... 7=pazar; 0 = belirtilmemiş */
    int saat;         /* 0-23; -1 = belirtilmemiş */
    int dakika;       /* 0-59; saat varsa anlamlı */
    char is[160];     /* zaman sözcükleri çıkarılmış, yapılacak iş metni */
} zaman_t;

/* Metinde zaman ifadesi varsa z'yi doldurur ve 1 döner; yoksa 0. */
int zaman_ayikla(const char *metin, zaman_t *z);
#endif

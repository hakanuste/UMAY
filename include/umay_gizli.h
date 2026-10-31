/* GİZLİLİK DUVARI: kişisel sözcük geçen hiçbir şey cihazdan dışarı çıkmaz.
 *
 * Dışarı çıkan her ağ isteği (web araştırması, çeviri, ULAK canlı veri) ağ kapısından geçerken
 * adresi çözülüp bu liste ile karşılaştırılır; eşleşirse istek HİÇ gönderilmez.
 * Liste iki parçadır:
 *   1) gömülü, genel sözcükler: şifre, parola, iban, tapu, parsel, maaş, miras, hisseli
 *   2) <kütüphane>/gizli.txt: kullanıcının kendi listesi (her satır bir sözcük; # yorum).
 *      Yer adları, sağlık, para ve mülk sözcükleri BURAYA yazılır; depoya (GitHub) GİRMEZ,
 *      çünkü kütüphane klasörü sürüm denetiminin dışındadır.
 * Eşleşme büyük/küçük harfe bakmaz ve sözcüğün parçasını da yakalar (ör. "tapu" -> "tapuda"). */
#ifndef UMAY_GIZLI_H
#define UMAY_GIZLI_H
#include "umay.h"

/* 1 = metinde gizli sözcük var (dışarı gönderme). Liste ilk kullanımda yüklenir. */
int gizli_mi(const char *metin);
/* Aynısı ama adres biçimli: %XX ve '+' çözülür. */
int gizli_adres_mi(const char *url);
/* Listeyi yeniden okur (kütüphane yolu değişince). Eklenen kullanıcı sözcüğü sayısını döner. */
int gizli_yukle(const char *kut);
#endif

/* DUVAR KOSUT CAGRI SINAMASI — "make duvar-kosut"
 *
 * NEDEN AYRI ARAC: sina takimi tek is parcaliydir ve ÖTÜKEN kipinde de
 * derlenir; pthread oraya girmez. Yarismayi gormek icin gercekten
 * kosut cagri gerekir, o yuzden bu sinama ayri durur.
 *
 * NE OLCULDU: gizli_mi ve gizli_adres_mi tarama arabelleklerini
 * DURAAGAN tutuyordu (static char k[2048], g[48], c[2048]). Iki is
 * parcasi ayni anda girince birbirinin arabelleğini bozuyordu ve
 * yanlis yanit veren bir gizlilik denetimi ACIK TARAFA duser:
 * gitmemesi gereken istek gider.
 *
 * 8 is parcasi x 4000 tur ile olculen:
 *     eski kod : 33943 kez gizli metni KACIRDI, 642 yanlis alarm
 *     yeni kod :     0  kacirma,                  0 yanlis alarm
 * Arabellekler yigina alindi; sozcuk listesi cift arabellekle
 * yayimlaniyor (satirlar yazilir, sonra isaretci, en sonunda sayi).
 *
 * Cikis kodu 0 = hic yanlis yanit yok. */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "umay_gizli.h"
#define IS 8
#define TUR 4000
static volatile int yanlis_var, yanlis_yok;
static char *g_uzun_var, *g_uzun_yok;
static void *kos(void *x){
    long id=(long)x;
    for(int i=0;i<TUR;i++){
        /* gizli OLAN metinler: 1 donmeli */
        if(gizli_mi("tapu kaydim")!=1) __sync_fetch_and_add(&yanlis_var,1);
        if(gizli_mi(g_uzun_var)!=1)    __sync_fetch_and_add(&yanlis_var,1);
        if(gizli_adres_mi("https://a.b/?q=%74apu")!=1) __sync_fetch_and_add(&yanlis_var,1);
        /* gizli OLMAYAN metinler: 0 donmeli */
        if(gizli_mi("bugun hava guzel")!=0) __sync_fetch_and_add(&yanlis_yok,1);
        if(gizli_mi(g_uzun_yok)!=0)         __sync_fetch_and_add(&yanlis_yok,1);
        (void)id;
    }
    return 0;
}
int main(void){
    gizli_yukle("/yok");
    g_uzun_var=malloc(40000); memset(g_uzun_var,'a',40000);
    for(int i=60;i<40000;i+=60) g_uzun_var[i]=' ';
    strcpy(g_uzun_var+39900," tapu");
    g_uzun_yok=malloc(40000); memset(g_uzun_yok,'b',40000);
    for(int i=60;i<40000;i+=60) g_uzun_yok[i]=' ';
    g_uzun_yok[39999]=0;
    pthread_t t[IS];
    for(long i=0;i<IS;i++) pthread_create(&t[i],0,kos,(void*)i);
    for(int i=0;i<IS;i++) pthread_join(t[i],0);
    printf("  %d is parcasi x %d tur = %d cagri kumesi\n",IS,TUR,IS*TUR);
    printf("  gizli OLAN metni kaciran (ACIK TARAFA DUSME): %d\n",yanlis_var);
    printf("  gizli OLMAYANI gizli sayan (yanlis alarm)   : %d\n",yanlis_yok);
    return (yanlis_var||yanlis_yok)?1:0;
}

#ifndef UMAY_HOST_H
#define UMAY_HOST_H
/* ANA BILGISAYAR UYUM KATMANI (Linux, macOS, Windows, Android/Termux).
 *
 * NEDEN: ollama, piper ve konsol arka uclari dogrudan POSIX cagiriyordu
 * (socket, popen, fcntl, "/tmp", ':' ile PATH). Bu, programi yalniz
 * Linux ve macOS'ta derlenebilir kiliyordu. Platform farki cekirdege
 * sizmasin diye hepsi burada, TEK dosyada (port/umay_host_uyum.c) durur;
 * ÖTÜKEN derlemesine GIRMEZ. Cekirdek kodda kosullu derleme YOKTUR. */
#include <stdint.h>
#include <stdio.h>

/* --- dizinler ve yollar --- */
const char *uh_ev(void);                               /* HOME / USERPROFILE */
int   uh_exe_dizini(char *cik, uint32_t n);            /* calisan programin dizini; 1 = bulundu */
void  uh_ortam_ata(const char *ad, const char *deger); /* ortam degiskeni koy */
void  uh_gecici(const char *ad, char *cik, uint32_t n);/* TMPDIR / TEMP + ad */
int   uh_yol_ara(const char *ad, char *cik, uint32_t n);/* PATH'te calistirilabilir */

/* --- dis komutlar --- */
void  uh_tirnak(const char *s, char *cik, uint32_t n); /* kabuk icin tirnakla */
const char *uh_sessiz(void);                           /* ">/dev/null 2>&1" ya da ">NUL 2>&1" */
FILE *uh_boru_ac(const char *komut);                   /* stdin'ine yazilan alt surec */
int   uh_boru_kapat(FILE *f);                          /* 0 = basarili */
int   uh_komut(const char *komut);                     /* 0 = basarili */
int   uh_pid(void);
int   uh_yerlesik_oynatici(void);                      /* platformun kendi WAV calicisi var mi */
int   uh_yerlesik_oynat(const char *wav);              /* 0 = calindi */

/* --- uçbirim --- */
void  uh_konsol_hazirla(void);                         /* UTF-8 + renk kacislari */
int   uh_renkli_mi(void);
void  uh_stdin_hazirla(void);                          /* bloklamayan okuma */
/* 1 = bir bayt okundu, 0 = su an veri yok, -1 = girdi bitti/hata */
int   uh_stdin_oku(char *c);

/* --- TCP (ollama) --- */
intptr_t uh_tcp_baglan(const char *ipv4, int kapi);    /* <0 = hata */
int64_t  uh_tcp_yaz(intptr_t h, const void *v, uint64_t n);
int64_t  uh_tcp_oku(intptr_t h, void *v, uint64_t n);
void     uh_tcp_kapat(intptr_t h);

#endif

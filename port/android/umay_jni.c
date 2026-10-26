/* Android (JNI) koprusu: Java/Kotlin -> include/umay_gomulu.h
 * Sinif: tr.umay.Umay  (port/android/tr/umay/Umay.java)
 * Dize aktarimi JNI "degistirilmis UTF-8" ile yapilir; Turkce harfler (BMP)
 * icin standart UTF-8 ile ayni baytlardir. */
#include <jni.h>
#include <stdlib.h>
#include "umay_gomulu.h"

#define YANIT_AZAMI 8192

JNIEXPORT jint JNICALL
Java_tr_umay_Umay_baslat(JNIEnv *env, jclass cls, jstring dizin)
{
    (void)cls;
    const char *d = dizin ? (*env)->GetStringUTFChars(env, dizin, 0) : 0;
    int r = umay_gomulu_baslat(d);
    if (d) (*env)->ReleaseStringUTFChars(env, dizin, d);
    return r;
}

/* Hata ya da bos soruda null doner. */
JNIEXPORT jstring JNICALL
Java_tr_umay_Umay_sor(JNIEnv *env, jclass cls, jstring soru)
{
    (void)cls;
    if (!soru) return 0;
    const char *s = (*env)->GetStringUTFChars(env, soru, 0);
    if (!s) return 0;
    char *c = (char *)malloc(YANIT_AZAMI);
    jstring sonuc = 0;
    if (c && umay_gomulu_sor(s, c, YANIT_AZAMI) >= 0) sonuc = (*env)->NewStringUTF(env, c);
    free(c);
    (*env)->ReleaseStringUTFChars(env, soru, s);
    return sonuc;
}

JNIEXPORT void JNICALL
Java_tr_umay_Umay_kapat(JNIEnv *env, jclass cls)
{
    (void)env; (void)cls;
    umay_gomulu_kapat();
}

JNIEXPORT jstring JNICALL
Java_tr_umay_Umay_surum(JNIEnv *env, jclass cls)
{
    (void)cls;
    return (*env)->NewStringUTF(env, umay_gomulu_surum());
}

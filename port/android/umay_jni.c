/* Android (JNI) koprusu: Java/Kotlin -> include/umay_gomulu.h
 * Sinif: tr.umay.Umay  (port/android/tr/umay/Umay.java)
 * Dize aktarimi JNI "degistirilmis UTF-8" ile yapilir; Turkce harfler (BMP)
 * icin standart UTF-8 ile ayni baytlardir. */
#include <jni.h>
#include <stdlib.h>
#include <string.h>
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

/* ---- web arastirmasi: Java tarafi agi getirir ----
 * Tr.umay.Umay.AgGetirici.getir(String) -> String (null = ag yok).
 * Cagri, Java'dan native'e giren AYNI iplikten gelir; env bu yuzden hazirdir. */
static JavaVM  *g_vm;
static jobject  g_getirici;
static jmethodID g_getir_mid;

static int64_t java_getir(const char *url, char *cik, uint32_t n)
{
    JNIEnv *env = 0;
    if (!g_vm || !g_getirici || n < 2) return -1;
    if ((*g_vm)->GetEnv(g_vm, (void **)&env, JNI_VERSION_1_6) != JNI_OK || !env) return -1;
    jstring u = (*env)->NewStringUTF(env, url);
    if (!u) return -1;
    jstring r = (jstring)(*env)->CallObjectMethod(env, g_getirici, g_getir_mid, u);
    (*env)->DeleteLocalRef(env, u);
    if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return -1; }
    if (!r) return -1;
    const char *c = (*env)->GetStringUTFChars(env, r, 0);
    int64_t k = -1;
    if (c) {
        size_t len = strlen(c);
        if (len > n - 1) len = n - 1;
        memcpy(cik, c, len);
        cik[len] = 0;
        k = (int64_t)len;
        (*env)->ReleaseStringUTFChars(env, r, c);
    }
    (*env)->DeleteLocalRef(env, r);
    return k;
}

JNIEXPORT void JNICALL
Java_tr_umay_Umay_agGetiriciAta(JNIEnv *env, jclass cls, jobject getirici)
{
    (void)cls;
    if (g_getirici) { (*env)->DeleteGlobalRef(env, g_getirici); g_getirici = 0; }
    if (!getirici) { umay_gomulu_ag_ata(0); return; }
    (*env)->GetJavaVM(env, &g_vm);
    g_getirici = (*env)->NewGlobalRef(env, getirici);
    jclass k = (*env)->GetObjectClass(env, getirici);
    g_getir_mid = (*env)->GetMethodID(env, k, "getir", "(Ljava/lang/String;)Ljava/lang/String;");
    if (!g_getir_mid) { (*env)->ExceptionClear(env); umay_gomulu_ag_ata(0); return; }
    umay_gomulu_ag_ata(java_getir);
}

/* konu null: bekleyen bosluklar. Negatif: ag yok/kapali. */
JNIEXPORT jint JNICALL
Java_tr_umay_Umay_ogren(JNIEnv *env, jclass cls, jstring konu)
{
    (void)cls;
    const char *k = konu ? (*env)->GetStringUTFChars(env, konu, 0) : 0;
    int r = umay_gomulu_ogren(k);
    if (k) (*env)->ReleaseStringUTFChars(env, konu, k);
    return r;
}

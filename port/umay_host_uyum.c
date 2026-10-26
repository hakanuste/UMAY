/* Ana bilgisayar uyum katmani — bkz. include/umay_host.h
 * Platform farklari YALNIZ burada durur. */
#define _POSIX_C_SOURCE 200809L
#include "umay_host.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <windows.h>
#  include <io.h>
#else
#  ifdef __APPLE__
#    include <mach-o/dyld.h>
#  endif
#  include <unistd.h>
#  include <fcntl.h>
#  include <sys/types.h>
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <netinet/tcp.h>
#  include <arpa/inet.h>
#endif

/* ===================================================================
 * dizinler ve yollar
 * =================================================================== */
const char *uh_ev(void)
{
    const char *e = getenv("HOME");
    if (e && *e) return e;
#ifdef _WIN32
    e = getenv("USERPROFILE");
    if (e && *e) return e;
#endif
    return 0;
}

int uh_exe_dizini(char *cik, uint32_t n)
{
    char yol[1024];
#ifdef _WIN32
    DWORD k = GetModuleFileNameA(0, yol, sizeof yol);
    if (k == 0 || k >= sizeof yol) return 0;
#elif defined(__APPLE__)
    uint32_t boy = sizeof yol;
    if (_NSGetExecutablePath(yol, &boy) != 0) return 0;
#else
    ssize_t k = readlink("/proc/self/exe", yol, sizeof yol - 1);
    if (k <= 0) return 0;
    yol[k] = 0;
#endif
    char *son = strrchr(yol, '/');
#ifdef _WIN32
    char *ters = strrchr(yol, '\\');
    if (ters && (!son || ters > son)) son = ters;
#endif
    if (!son) return 0;
    *son = 0;
    if (cik && n) snprintf(cik, n, "%s", yol);
    return 1;
}

void uh_ortam_ata(const char *ad, const char *deger)
{
#ifdef _WIN32
    _putenv_s(ad, deger);
#else
    setenv(ad, deger, 1);
#endif
}

void uh_gecici(const char *ad, char *cik, uint32_t n)
{
    const char *d = getenv("TMPDIR");
#ifdef _WIN32
    if (!d || !*d) d = getenv("TEMP");
    if (!d || !*d) d = getenv("TMP");
    if (!d || !*d) d = ".";
#else
    /* Android'de /tmp yoktur; Termux TMPDIR verir. */
    if (!d || !*d) d = "/tmp";
#endif
    snprintf(cik, n, "%s/%s", d, ad);
}

static int dosya_mi(const char *y)
{
    struct stat s;
    return stat(y, &s) == 0 && S_ISREG(s.st_mode);
}

int uh_yol_ara(const char *ad, char *cik, uint32_t n)
{
    const char *yol = getenv("PATH");
    if (!ad || !*ad || !yol) return 0;
#ifdef _WIN32
    const char ayirac = ';';
    static const char *uzanti[] = { "", ".exe", ".cmd", ".bat", 0 };
#else
    const char ayirac = ':';
    static const char *uzanti[] = { "", 0 };
#endif
    const char *p = yol;
    for (;;) {
        const char *son = strchr(p, ayirac);
        size_t k = son ? (size_t)(son - p) : strlen(p);
        if (k > 0) {
            for (int u = 0; uzanti[u]; u++) {
                char t[1024];
                int w = snprintf(t, sizeof t, "%.*s/%s%s", (int)k, p, ad, uzanti[u]);
                if (w <= 0 || (size_t)w >= sizeof t) continue;
                if (!dosya_mi(t)) continue;
#ifndef _WIN32
                if (access(t, X_OK) != 0) continue;
#endif
                if (cik && n) snprintf(cik, n, "%s", t);
                return 1;
            }
        }
        if (!son) break;
        p = son + 1;
    }
    return 0;
}

/* ===================================================================
 * dis komutlar
 * =================================================================== */
void uh_tirnak(const char *s, char *cik, uint32_t n)
{
    uint32_t o = 0;
    if (!n) return;
#ifdef _WIN32
    if (o + 1 < n) cik[o++] = '"';
    for (; s && *s && o + 2 < n; s++) {
        if (*s == '"') cik[o++] = '\\';
        cik[o++] = *s;
    }
    if (o + 1 < n) cik[o++] = '"';
#else
    /* Tek tirnak: icindeki ' karakteri '\'' olur. */
    if (o + 1 < n) cik[o++] = '\'';
    for (; s && *s && o + 5 < n; s++) {
        if (*s == '\'') { memcpy(cik + o, "'\\''", 4); o += 4; }
        else cik[o++] = *s;
    }
    if (o + 1 < n) cik[o++] = '\'';
#endif
    cik[o] = 0;
}

const char *uh_sessiz(void)
{
#ifdef _WIN32
    return ">NUL 2>&1";
#else
    return ">/dev/null 2>&1";
#endif
}

/* cmd.exe /c, komut " ile basliyorsa en bastaki ve en sondaki tirnagi
 * soyar; bu yuzden Windows'ta komutun tamami bir kat daha tirnaklanir. */
#define KOMUT_AZAMI 16384
static int kabuk_komutu(const char *komut, char *cik, uint32_t n)
{
#ifdef _WIN32
    int w = snprintf(cik, n, "\"%s\"", komut);
#else
    int w = snprintf(cik, n, "%s", komut);
#endif
    return (w >= 0 && (uint32_t)w < n) ? 0 : -1;     /* kirpildiysa -1 */
}

FILE *uh_boru_ac(const char *komut)
{
    char k[KOMUT_AZAMI];
    if (kabuk_komutu(komut, k, sizeof k) != 0) return 0;
#ifdef _WIN32
    return _popen(k, "w");
#else
    return popen(k, "w");
#endif
}

int uh_boru_kapat(FILE *f)
{
#ifdef _WIN32
    return _pclose(f);
#else
    return pclose(f);
#endif
}

int uh_komut(const char *komut)
{
    char k[KOMUT_AZAMI];
    if (kabuk_komutu(komut, k, sizeof k) != 0) return -1;
    return system(k);
}

int uh_pid(void)
{
#ifdef _WIN32
    return (int)GetCurrentProcessId();
#else
    return (int)getpid();
#endif
}

int uh_yerlesik_oynatici(void)
{
#ifdef _WIN32
    return uh_yol_ara("powershell", 0, 0);
#else
    return 0;
#endif
}

int uh_yerlesik_oynat(const char *wav)
{
#ifdef _WIN32
    char k[2048];
    /* PowerShell dizgesi tek tirnakla; cmd'e giden kisim cift tirnakli. */
    snprintf(k, sizeof k,
             "powershell -NoProfile -NonInteractive -Command "
             "\"(New-Object Media.SoundPlayer '%s').PlaySync()\" %s",
             wav, uh_sessiz());
    return uh_komut(k) == 0 ? 0 : -1;
#else
    (void)wav;
    return -1;
#endif
}

/* ===================================================================
 * uçbirim
 * =================================================================== */
#ifdef _WIN32
static int g_vt;
#endif

void uh_konsol_hazirla(void)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    HANDLE o = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD kip = 0;
    if (o != INVALID_HANDLE_VALUE && GetConsoleMode(o, &kip))
        g_vt = SetConsoleMode(o, kip | 0x0004 /* ENABLE_VIRTUAL_TERMINAL_PROCESSING */) ? 1 : 0;
#endif
}

int uh_renkli_mi(void)
{
#ifdef _WIN32
    return g_vt;
#else
    const char *term = getenv("TERM");
    return isatty(STDOUT_FILENO) && term && strcmp(term, "dumb") != 0;
#endif
}

#ifdef _WIN32
/* Windows'ta bloklamayan stdin yoktur (boru ve konsol farkli davranir).
 * Ayri bir is parcacigi bloklayarak okur, halka tampona koyar. */
#define STDIN_TAMPON 8192
static CRITICAL_SECTION g_kilit;
static char     g_tampon[STDIN_TAMPON];
static unsigned g_bas, g_son;
static volatile int g_bitti, g_basladi;

static DWORD WINAPI stdin_isi(LPVOID arg)
{
    (void)arg;
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    char blok[256];
    DWORD k;
    for (;;) {
        if (!ReadFile(h, blok, sizeof blok, &k, 0) || k == 0) break;
        for (DWORD i = 0; i < k; i++) {
            for (;;) {
                EnterCriticalSection(&g_kilit);
                unsigned sonraki = (g_son + 1) % STDIN_TAMPON;
                int yer = sonraki != g_bas;
                if (yer) { g_tampon[g_son] = blok[i]; g_son = sonraki; }
                LeaveCriticalSection(&g_kilit);
                if (yer) break;
                Sleep(5);                        /* tuketici yetissin */
            }
        }
    }
    g_bitti = 1;
    return 0;
}
#else
static int g_nb;
#endif

void uh_stdin_hazirla(void)
{
#ifdef _WIN32
    if (g_basladi) return;
    g_basladi = 1;
    InitializeCriticalSection(&g_kilit);
    HANDLE t = CreateThread(0, 0, stdin_isi, 0, 0, 0);
    if (t) CloseHandle(t); else g_bitti = 1;
#else
    int bayrak = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (bayrak != -1) { fcntl(STDIN_FILENO, F_SETFL, bayrak | O_NONBLOCK); g_nb = 1; }
#endif
}

int uh_stdin_oku(char *c)
{
#ifdef _WIN32
    if (!g_basladi) return -1;
    int r = 0;
    EnterCriticalSection(&g_kilit);
    if (g_bas != g_son) { *c = g_tampon[g_bas]; g_bas = (g_bas + 1) % STDIN_TAMPON; r = 1; }
    LeaveCriticalSection(&g_kilit);
    if (r == 1) return 1;
    return g_bitti ? -1 : 0;
#else
    /* O_NONBLOCK acikken veri YOKKEN read() -1/EAGAIN doner; o EOF degildir.
     * 0 doner ise girdi bitmistir. */
    ssize_t r = read(STDIN_FILENO, c, 1);
    if (r == 1) return 1;
    if (r == 0) return -1;
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return 0;
    return -1;
#endif
}

/* ===================================================================
 * TCP
 * =================================================================== */
intptr_t uh_tcp_baglan(const char *ipv4, int kapi)
{
#ifdef _WIN32
    static int wsa;
    if (!wsa) {
        WSADATA d;
        if (WSAStartup(MAKEWORD(2, 2), &d) != 0) return -1;
        wsa = 1;
    }
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET) return -1;
#else
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return -1;
#endif
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons((uint16_t)kapi);
    if (inet_pton(AF_INET, (ipv4 && *ipv4) ? ipv4 : "127.0.0.1", &a.sin_addr) != 1 ||
        connect(s, (struct sockaddr *)&a, sizeof a) != 0) {
        uh_tcp_kapat((intptr_t)s);
        return -1;
    }
    int bir = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&bir, sizeof bir);
    return (intptr_t)s;
}

int64_t uh_tcp_yaz(intptr_t h, const void *v, uint64_t n)
{
#ifdef _WIN32
    return send((SOCKET)h, (const char *)v, (int)n, 0);
#else
    return write((int)h, v, (size_t)n);
#endif
}

int64_t uh_tcp_oku(intptr_t h, void *v, uint64_t n)
{
#ifdef _WIN32
    return recv((SOCKET)h, (char *)v, (int)n, 0);
#else
    return read((int)h, v, (size_t)n);
#endif
}

void uh_tcp_kapat(intptr_t h)
{
#ifdef _WIN32
    closesocket((SOCKET)h);
#else
    close((int)h);
#endif
}

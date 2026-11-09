/* up_ayir/up_biraktir'in sade karsiligi: ASan'in her ayirmanin
 * sinirini denetleyebilmesi icin dogrudan malloc/free. Cekirdegin
 * kendi ayiricisi bir havuzdan verirse ASan tasmayi goremez. */
#include <stdlib.h>
#include <stdint.h>
void *up_ayir(uint64_t n) { return malloc((size_t)n); }
void  up_biraktir(void *p) { free(p); }

/* Varsayılan sağlayıcı: bu yapıda gömülü motor yok. Motorlu yapıda bu dosya
 * yerine motor/umay_motor.cpp bağlanır (arac/motor.sh). */
#include "umay_motor.h"
const umay_motor_api_t *umay_motor_api(void) { return 0; }

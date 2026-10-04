// Provide mbedtls_ms_time() for bare metal Pico (C implementation for mbedtls build)
#include "pico/time.h"

int64_t mbedtls_ms_time(void) {
    return to_ms_since_boot(get_absolute_time());
}
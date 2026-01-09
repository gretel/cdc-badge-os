#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool i2c_ok;
    bool power_ok;
    bool keypad_ok;
    bool tropic01_ok;
    bool tropic01_session_ok;
} hw_status_t;

// Build selftest text for UI rendering.
void system_status_build_selftest_text(const hw_status_t *status, char *buf, size_t buf_size);

#ifdef __cplusplus
}
#endif


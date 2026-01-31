#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// FIDO2 ClientPIN support (badge PIN hash)
bool pin_storage_fido2_available(void);
bool pin_storage_get_fido2_hash(uint8_t* hash_out);
bool pin_storage_verify_fido2_hash(const uint8_t* hash_in);

// Badge PIN status
bool pin_storage_is_set(void);

#ifdef __cplusplus
}
#endif


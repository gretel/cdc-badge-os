#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TR01_CACHE_ECC_SLOTS    32
#define TR01_CACHE_RMEM_SLOTS   256
#define TR01_CACHE_DATA_SIZE    256  // Must be >= FIDO2 stored struct (227 bytes)

// Initialize cache by loading all data from TROPIC01 (call once at boot)
void tropic01_cache_init(void);

// Check if cache is loaded
bool tropic01_cache_is_loaded(void);

// ECC cache queries (no TROPIC01 access)
bool tropic01_cache_ecc_exists(uint8_t slot);
bool tropic01_cache_ecc_get_pubkey(uint8_t slot, uint8_t *pubkey, uint8_t *curve);
uint8_t tropic01_cache_ecc_count(void);

// ECC cache updates (call after TROPIC01 write)
void tropic01_cache_ecc_update(uint8_t slot, const uint8_t *pubkey, uint8_t curve);
void tropic01_cache_ecc_invalidate(uint8_t slot);

// R-Memory cache queries (no TROPIC01 access)
bool tropic01_cache_rmem_exists(uint16_t slot);
bool tropic01_cache_rmem_get(uint16_t slot, uint8_t *data, uint16_t *size);
uint16_t tropic01_cache_rmem_count_range(uint16_t start, uint16_t end);

// R-Memory cache updates (call after TROPIC01 write)
void tropic01_cache_rmem_update(uint16_t slot, const uint8_t *data, uint16_t size);
void tropic01_cache_rmem_invalidate(uint16_t slot);

#ifdef __cplusplus
}
#endif

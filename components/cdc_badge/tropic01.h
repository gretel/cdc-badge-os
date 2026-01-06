#pragma once

#include <stdint.h>
#include <stdbool.h>

// R-Memory slot definitions
#define TR01_RMEM_SLOT_PIN      30    // PIN hash storage
#define TR01_RMEM_SLOT_CONFIG   31    // Device config

// Initialize TROPIC01 secure element
bool tropic01_init(void);

// Start secure session (required before R-Memory access)
bool tropic01_session_start(void);

// Check if session is active
bool tropic01_session_active(void);

// Put TROPIC01 to sleep (call when idle)
void tropic01_sleep(void);

// R-Memory operations
bool tropic01_rmem_read(uint16_t slot, uint8_t *data, uint16_t max_size, uint16_t *read_size);
bool tropic01_rmem_write(uint16_t slot, const uint8_t *data, uint16_t size);
bool tropic01_rmem_erase(uint16_t slot);

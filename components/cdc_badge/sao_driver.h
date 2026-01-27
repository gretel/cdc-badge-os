#pragma once

// SAO Driver Interface
// Demo interface for future SAO driver implementations
// NOT IMPLEMENTED - only defines the interface structure

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sao_driver sao_driver_t;

// Driver operations interface
typedef struct {
    // Initialize driver after SAO detection
    // driver_data: from binary descriptor, data_len: length of driver data
    bool (*init)(sao_driver_t *drv, const uint8_t *driver_data, uint8_t data_len);

    // Deinitialize when SAO is removed
    void (*deinit)(sao_driver_t *drv);

    // Periodic update (for LEDs, animations, etc.)
    // Returns ms until next update call
    uint32_t (*update)(sao_driver_t *drv);

    // Key input handling (optional)
    // key: keypress from badge keypad
    // Returns true if handled
    bool (*on_key)(sao_driver_t *drv, char key);

    // Custom rendering (optional)
    // Called when SAO-specific view is active
    void (*render)(sao_driver_t *drv, bool partial);
} sao_driver_ops_t;

// Driver instance
struct sao_driver {
    const char *name;           // "neopixel", "ssd1306", etc.
    const sao_driver_ops_t *ops;
    void *user_data;            // Driver-specific data
    bool initialized;
};

// Driver registry functions (NOT IMPLEMENTED)
// Register a driver for a specific driver name
void sao_driver_register(const char *driver_name, const sao_driver_ops_t *ops);

// Find driver by name (from binary descriptor)
const sao_driver_ops_t* sao_driver_find(const char *driver_name);

// Initialize active driver
bool sao_driver_init_active(const char *driver_name, const uint8_t *data, uint8_t len);

// Deinitialize active driver
void sao_driver_deinit_active(void);

// Call update on active driver
uint32_t sao_driver_update(void);

// Forward keypress to active driver
bool sao_driver_on_key(char key);

#ifdef __cplusplus
}
#endif

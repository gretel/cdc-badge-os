#pragma once

// Grove Driver Interface
// Demo interface for GPIO-based Grove modules (Digital, MY9221, etc.)
// NOT IMPLEMENTED - only defines the interface structure

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct grove_driver grove_driver_t;

// Driver operations interface
typedef struct {
    // Initialize with GPIO pins
    bool (*init)(grove_driver_t *drv, gpio_num_t pin1, gpio_num_t pin2);

    // Deinitialize
    void (*deinit)(grove_driver_t *drv);

    // Periodic update
    // Returns ms until next update call
    uint32_t (*update)(grove_driver_t *drv);

    // Key input handling (optional)
    bool (*on_key)(grove_driver_t *drv, char key);

    // Custom rendering (optional)
    void (*render)(grove_driver_t *drv, bool partial);
} grove_driver_ops_t;

// Driver instance
struct grove_driver {
    const char *name;           // "circular_led", "button", etc.
    gpio_num_t pin1;            // Grove0 (IO2)
    gpio_num_t pin2;            // Grove1 (IO3)
    const grove_driver_ops_t *ops;
    void *user_data;            // Driver-specific data
    bool initialized;
};

#ifdef __cplusplus
}
#endif

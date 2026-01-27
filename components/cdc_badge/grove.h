#pragma once

// Grove Port Interface
// Basic status and info functions for Grove connector
// No detection protocol available - Grove modules have no EEPROM
//
// Grove port on CDC Badge:
// - Pin 1 (Yellow): Grove0 (ESP32 IO2)
// - Pin 2 (White):  Grove1 (ESP32 IO3)
// - Pin 3 (Red):    VCC (3.3V)
// - Pin 4 (Black):  GND
//
// The Grove port uses flexible GPIO pins that can be configured as:
// - Digital I/O
// - Analog input
// - UART (with software configuration)
// - I2C (requires external pull-ups)

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool initialized;
    bool pins_configured;   // Pins are configured for output
} grove_status_t;

// Initialize Grove port (configure GPIO pins)
bool grove_init(void);

// Check if Grove port is available
bool grove_is_available(void);

// Get status info
void grove_get_status(grove_status_t *status);

// Generate info string for display
void grove_get_info_string(char *buf, size_t len);

#ifdef __cplusplus
}
#endif

#pragma once

// Example: NeoPixel SAO Driver
// NOT IMPLEMENTED - only as template for future implementation

#include "sao_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

// Register the NeoPixel driver
// Call in main.cpp or sao.cpp during initialization
void sao_driver_neopixel_register(void);

/*
 * Expected driver_data format (from binary descriptor):
 * Byte 0: Number of LEDs
 * Byte 1: LED type (WS2812, SK6812, etc.)
 * Byte 2: GPIO pin (0 = SAO_GPIO1, 1 = SAO_GPIO2)
 *
 * Example implementation (NOT INCLUDED):
 *
 * typedef struct {
 *     uint8_t num_leds;
 *     uint8_t led_type;
 *     gpio_num_t gpio_pin;
 *     uint8_t *led_buffer;
 *     uint8_t brightness;
 *     uint8_t animation_mode;
 * } neopixel_state_t;
 *
 * static bool neopixel_init(sao_driver_t *drv, const uint8_t *data, uint8_t len) {
 *     if (len < 3) return false;
 *     neopixel_state_t *state = malloc(sizeof(neopixel_state_t));
 *     state->num_leds = data[0];
 *     state->led_type = data[1];
 *     state->gpio_pin = (data[2] == 0) ? SAO_GPIO1_PIN : SAO_GPIO2_PIN;
 *     state->led_buffer = malloc(state->num_leds * 3);
 *     // Initialize RMT/LED strip driver...
 *     drv->user_data = state;
 *     return true;
 * }
 *
 * static void neopixel_deinit(sao_driver_t *drv) {
 *     neopixel_state_t *state = drv->user_data;
 *     // Turn off LEDs, release RMT channel...
 *     free(state->led_buffer);
 *     free(state);
 * }
 *
 * static uint32_t neopixel_update(sao_driver_t *drv) {
 *     neopixel_state_t *state = drv->user_data;
 *     // Update animation, write LEDs
 *     return 50; // Next update in 50ms
 * }
 *
 * static bool neopixel_on_key(sao_driver_t *drv, char key) {
 *     neopixel_state_t *state = drv->user_data;
 *     if (key == '1') { state->animation_mode++; return true; }
 *     if (key == '4') { state->brightness = (state->brightness + 32) % 256; return true; }
 *     return false;
 * }
 *
 * static const sao_driver_ops_t neopixel_ops = {
 *     .init = neopixel_init,
 *     .deinit = neopixel_deinit,
 *     .update = neopixel_update,
 *     .on_key = neopixel_on_key,
 *     .render = NULL,
 * };
 *
 * void sao_driver_neopixel_register(void) {
 *     sao_driver_register("neopixel", &neopixel_ops);
 * }
 */

#ifdef __cplusplus
}
#endif

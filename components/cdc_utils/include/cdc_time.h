#pragma once

#include <stdint.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static inline uint32_t cdc_millis(void) {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static inline uint64_t cdc_micros(void) {
    return (uint64_t)esp_timer_get_time();
}

static inline void cdc_delay_ms(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

#define millis() cdc_millis()
#define micros() cdc_micros()
#define delay(ms) cdc_delay_ms(ms)

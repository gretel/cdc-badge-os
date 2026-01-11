#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize internal temperature sensor (lazy in getter).
bool temp_sensor_init(void);

// Read temperature in Celsius. Returns true on success.
bool temp_sensor_get_celsius(float *out_celsius);

#ifdef __cplusplus
}
#endif

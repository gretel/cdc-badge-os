#pragma once

#include <cstdint>

void enter_light_sleep(void);
void enter_deep_sleep(void);
uint16_t brightness_step(uint16_t current, bool up);


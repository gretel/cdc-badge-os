#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void pin_storage_openpgp_init(void);
bool pin_storage_openpgp_verify_pw1(const char *pin);
bool pin_storage_openpgp_verify_pw3(const char *pin);
bool pin_storage_openpgp_change_pw1(const char *new_pin);
bool pin_storage_openpgp_change_pw3(const char *new_pin);
uint8_t pin_storage_openpgp_pw1_retries(void);
uint8_t pin_storage_openpgp_pw3_retries(void);
void pin_storage_openpgp_reset_pw1_retries(void);
void pin_storage_openpgp_reset_pw3_retries(void);
bool pin_storage_openpgp_pw1_blocked(void);
bool pin_storage_openpgp_pw3_blocked(void);
bool pin_storage_openpgp_reset(void);

#ifdef __cplusplus
}
#endif

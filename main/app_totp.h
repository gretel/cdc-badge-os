#pragma once

#include "app_globals.h"

#if FEATURE_TOTP
void go_to_totp_list(void);
void show_totp_code(uint8_t index);
void totp_wizard_start(void);
void totp_wizard_edit(uint8_t index);
void totp_wizard_next_from_name(void);
void totp_wizard_next_from_secret(void);
void totp_wizard_next_from_issuer(void);
void totp_wizard_next_from_digits(uint8_t selection);
void totp_wizard_next_from_algo(uint8_t selection);
void totp_wizard_finish(uint8_t selection);
#endif


#pragma once

#include "feature_flags.h"

#if FEATURE_PASSWORD
#include <stdint.h>

void go_to_password_list(void);
void password_show_detail(uint16_t slot);

void password_wizard_start(void);
void password_wizard_edit(uint16_t slot);
void password_wizard_next_from_name(void);
void password_wizard_next_from_username(void);
void password_wizard_next_from_url(void);
void password_wizard_next_from_password(void);
void password_wizard_finish(void);
#endif

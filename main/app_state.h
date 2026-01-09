#pragma once

#include "app_globals.h"

void go_to_pin_entry(void);
void go_to_main_menu(void);
void go_to_settings_menu(void);
void go_to_badge_texts_menu(void);
void go_to_lock_screen(void);
#if !DEBUG_MODE
void go_to_lockout(void);
#endif


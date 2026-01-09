#pragma once

#include "app_globals.h"

#if FEATURE_FIDO2
void go_to_fido_list(void);
void show_fido_detail(uint8_t index);
void build_fido_context_menu(void);
fido2_user_presence_result_t fido2_user_presence_callback(
    const char *rp_id,
    fido2_action_t action,
    const char *user_name
);
void fido2_prompt_complete(fido2_user_presence_result_t result);
#endif


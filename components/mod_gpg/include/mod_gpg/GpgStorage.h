#pragma once

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

void gpg_storage_set_slot_range(uint16_t eccStart, uint16_t eccEnd);
bool gpg_storage_ready(void);
uint8_t gpg_storage_sig_slot(void);
uint8_t gpg_storage_dec_slot(void);
uint8_t gpg_storage_aut_slot(void);

#ifdef __cplusplus
}
#endif

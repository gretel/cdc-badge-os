#include "mod_gpg/GpgStorage.h"

static struct {
    bool ready = false;
    uint16_t eccStart = 0;
    uint16_t eccEnd = 0;
    uint8_t sigSlot = 0;
    uint8_t decSlot = 0;
    uint8_t autSlot = 0;
} s_storage;

void gpg_storage_set_slot_range(uint16_t eccStart, uint16_t eccEnd) {
    s_storage.ready = false;
    s_storage.eccStart = eccStart;
    s_storage.eccEnd = eccEnd;

    if (eccStart == 0 || eccEnd == 0) {
        return;
    }
    if (eccStart > eccEnd) {
        return;
    }
    if ((eccEnd - eccStart + 1) < 3) {
        return;
    }

    s_storage.sigSlot = static_cast<uint8_t>(eccStart);
    s_storage.decSlot = static_cast<uint8_t>(eccStart + 1);
    s_storage.autSlot = static_cast<uint8_t>(eccStart + 2);
    s_storage.ready = true;
}

bool gpg_storage_ready(void) {
    return s_storage.ready;
}

uint8_t gpg_storage_sig_slot(void) {
    return s_storage.sigSlot;
}

uint8_t gpg_storage_dec_slot(void) {
    return s_storage.decSlot;
}

uint8_t gpg_storage_aut_slot(void) {
    return s_storage.autSlot;
}

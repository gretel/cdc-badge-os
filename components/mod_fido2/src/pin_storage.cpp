// Pin storage adapter for FIDO2 ClientPIN

#include "mod_fido2/pin_storage.h"
#include "cdc_core/PinManager.h"

extern "C" {

bool pin_storage_fido2_available(void) {
    auto& pm = cdc::core::PinManager::instance();
    pm.init();
    uint8_t hash[cdc::core::PinManager::BADGE_HASH_SIZE] = {};
    return pm.getBadgePinHash(hash);
}

bool pin_storage_get_fido2_hash(uint8_t* hash_out) {
    auto& pm = cdc::core::PinManager::instance();
    pm.init();
    return pm.getBadgePinHash(hash_out);
}

bool pin_storage_verify_fido2_hash(const uint8_t* hash_in) {
    auto& pm = cdc::core::PinManager::instance();
    pm.init();
    return pm.verifyBadgePinHash(hash_in);
}

bool pin_storage_is_set(void) {
    auto& pm = cdc::core::PinManager::instance();
    pm.init();
    return pm.isPinSet();
}

} // extern "C"

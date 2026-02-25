/**
 * \file
 * \brief Adapter exposing badge PIN hash operations for FIDO2 ClientPIN logic.
 */

#include "mod_fido2/pin_storage.h"
#include "cdc_core/PinManager.h"

extern "C" {

/**
 * \brief Checks whether a FIDO2-compatible PIN hash is available.
 * \return `true` when hash is available, otherwise `false`.
 */
bool pin_storage_fido2_available(void) {
    auto& pm = cdc::core::PinManager::instance();
    pm.init();
    uint8_t hash[cdc::core::PinManager::BADGE_HASH_SIZE] = {};
    return pm.getBadgePinHash(hash);
}

/**
 * \brief Reads persisted FIDO2 PIN hash.
 * \param hash_out Destination buffer for hash bytes.
 * \return `true` on success, otherwise `false`.
 */
bool pin_storage_get_fido2_hash(uint8_t* hash_out) {
    auto& pm = cdc::core::PinManager::instance();
    pm.init();
    return pm.getBadgePinHash(hash_out);
}

/**
 * \brief Verifies an incoming hash against stored badge PIN hash.
 * \param hash_in Hash to verify.
 * \return `true` when hash matches, otherwise `false`.
 */
bool pin_storage_verify_fido2_hash(const uint8_t* hash_in) {
    auto& pm = cdc::core::PinManager::instance();
    pm.init();
    return pm.verifyBadgePinHash(hash_in);
}

/**
 * \brief Indicates whether a badge PIN is currently configured.
 * \return `true` when PIN is set, otherwise `false`.
 */
bool pin_storage_is_set(void) {
    auto& pm = cdc::core::PinManager::instance();
    pm.init();
    return pm.isPinSet();
}

} // extern "C"

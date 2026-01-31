#include "pin_storage.h"
#include "cdc_core/PinManager.h"

void pin_storage_openpgp_init(void) {
    cdc::core::PinManager::instance().init();
}

bool pin_storage_openpgp_verify_pw1(const char *pin) {
    return cdc::core::PinManager::instance().verifyPW1(pin);
}

bool pin_storage_openpgp_verify_pw3(const char *pin) {
    return cdc::core::PinManager::instance().verifyPW3(pin);
}

bool pin_storage_openpgp_change_pw1(const char *new_pin) {
    return cdc::core::PinManager::instance().setPW1(new_pin);
}

bool pin_storage_openpgp_change_pw3(const char *new_pin) {
    return cdc::core::PinManager::instance().setPW3(new_pin);
}

uint8_t pin_storage_openpgp_pw1_retries(void) {
    return cdc::core::PinManager::instance().getPW1Retries();
}

uint8_t pin_storage_openpgp_pw3_retries(void) {
    return cdc::core::PinManager::instance().getPW3Retries();
}

void pin_storage_openpgp_reset_pw1_retries(void) {
    cdc::core::PinManager::instance().resetPW1Retries();
}

void pin_storage_openpgp_reset_pw3_retries(void) {
    cdc::core::PinManager::instance().resetPW3Retries();
}

bool pin_storage_openpgp_pw1_blocked(void) {
    return cdc::core::PinManager::instance().isPW1Blocked();
}

bool pin_storage_openpgp_pw3_blocked(void) {
    return cdc::core::PinManager::instance().isPW3Blocked();
}

bool pin_storage_openpgp_reset(void) {
    bool ok1 = cdc::core::PinManager::instance().setPW1(cdc::core::PinManager::DEFAULT_PW1);
    bool ok3 = cdc::core::PinManager::instance().setPW3(cdc::core::PinManager::DEFAULT_PW3);
    cdc::core::PinManager::instance().resetPW1Retries();
    cdc::core::PinManager::instance().resetPW3Retries();
    return ok1 && ok3;
}

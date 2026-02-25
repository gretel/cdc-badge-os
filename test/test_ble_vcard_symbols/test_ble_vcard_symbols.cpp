#include "mod_vcard/ble_vcard.h"
#include "../../components/mod_vcard/src/ble_vcard.cpp"

/**
 * \brief Link/symbol smoke test for BLE vCard API.
 * \return void
 */
void test_ble_vcard_symbols() {
    ble_vcard_init();
    ble_vcard_set_exchange_enabled(true);
}

/**
 * \brief Test entry point.
 * \return void
 */
extern "C" void app_main() {
    test_ble_vcard_symbols();
}

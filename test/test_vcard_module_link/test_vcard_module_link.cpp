#include "mod_vcard/VcardModule.h"

extern "C" void mod_vcard_register();

/**
 * \brief Link/symbol smoke test for vCard module registration.
 * \return void
 */
void test_vcard_module_link() {
    mod_vcard_register();
}

/**
 * \brief Test entry point.
 * \return void
 */
extern "C" void app_main() {
    test_vcard_module_link();
}

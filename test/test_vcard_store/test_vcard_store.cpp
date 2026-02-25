#include "mod_vcard/vcard_store.h"
#include "../../components/mod_vcard/src/vcard_store.cpp"
#include <cstring>

/**
 * \brief Smoke-test for vCard validation/store path.
 * \return void
 */
void test_vcard_validate() {
    char err[64];
    const char* v = "BEGIN:VCARD\nVERSION:4.0\nFN:Test\nEND:VCARD\n";
    vcard_store_set_own(v, strlen(v), err, sizeof(err));
}

/**
 * \brief Test entry point.
 * \return void
 */
extern "C" void app_main() {
    test_vcard_validate();
}

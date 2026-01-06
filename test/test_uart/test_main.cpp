#include "../test_common.h"

// TODO: should be moved into a common lib later
void set_charging_ic2(void) {
    Serial.println("[CONFIG] Setting defaults on BQ25895");
    uint8_t readback;

    // Disable ILIM pin on REG00[6]
    uint8_t current_val = read_bq_reg(0x00);
    uint8_t new_val = current_val & ~(1 << 6);
    if (current_val != new_val) {
        write_bq_reg(0x00, new_val);
        Serial.printf("REG00: ILIM disabled. Old: 0x%02X -> New: 0x%02X\n", current_val, new_val);
    } else {
        Serial.println("REG00: ILIM was already disabled.");
    }

    // Disable OTG on REG03[5]
    current_val = read_bq_reg(0x03);
    new_val = current_val & ~(1 << 5);
    if (current_val != new_val) {
        write_bq_reg(0x03, new_val);
        Serial.printf("REG03: OTG disabled. Old: 0x%02X -> New: 0x%02X\n", current_val, new_val);
    } else {
        Serial.println("REG03: OTG was already disabled.");
    }
}

// DSEL test
void test_dsel(void) {
    Serial.println("[TEST] Testing charing IC settings and DSEL");
    // waits for user input (dcel pin low)
    // when low: turn usb off
}

void setup() {
    Serial.println("These tests are intended for UART, since they will turn OFF USB!");
    UNITY_BEGIN();

    setup_clean();
    setup_usb_serial();
    bq25895_init();

    RUN_TEST(set_charging_ic2);
    RUN_TEST(test_dsel);

    UNITY_END();
}

void loop() {
    if (UNITY_END()) {
        delay(1000);
    }
}

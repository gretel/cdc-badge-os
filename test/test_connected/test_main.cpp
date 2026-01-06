#include "../test_common.h"

// make sure to run tests with verbose mode -v
void test_serial_communication(void) {
    Serial.println("[TEST] USB Serial OK?");
    TEST_ASSERT_TRUE(true);
}

void test_gpio_led_output(void) {
    Serial.println("[TEST] 5 second LED test");
    pinMode(LED_PIN, OUTPUT);

    digitalWrite(LED_PIN, LOW);
    TEST_MESSAGE("-> LED should be ON");
    delay(5000);

    digitalWrite(LED_PIN, HIGH);
    TEST_MESSAGE("-> LED should be OFF");
    delay(5000);

    TEST_ASSERT_EQUAL(HIGH, digitalRead(LED_PIN));
}

void test_EPD_light(void) {
    Serial.println("[TEST] EPD background light");
    pinMode(EPD_LED_PIN, OUTPUT);

    digitalWrite(EPD_LED_PIN, LOW);
    digitalWrite(EPD_LED_PIN, HIGH);
    TEST_MESSAGE("-> EPD LED ON");
    delay(2000);

    digitalWrite(EPD_LED_PIN, LOW);
    TEST_MESSAGE("-> EPD LED OFF");
    delay(2000);

    digitalWrite(EPD_LED_PIN, HIGH);
    TEST_MESSAGE("-> EPD LED ON");
    delay(2000);

    digitalWrite(EPD_LED_PIN, LOW);
    TEST_MESSAGE("-> EPD LED OFF");
    delay(2000);

    TEST_ASSERT_TRUE(true);
}

void test_EPD_pwm(void) {
    Serial.println("[TEST] EPD background light (PWM Ramp Test)");

    const int PWM_CHANNEL = 0;     // LEDC-channel 0
    const int PWM_FREQ = 10000;    // 5 kHz freq
    const int PWM_RESOLUTION = 10; // 10 bit

    ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);

    ledcAttachPin(EPD_LED_PIN, PWM_CHANNEL);
    TEST_MESSAGE("PWM-Controller configured for EPD LED");

    TEST_MESSAGE("-> ramp 0% to 100%.");
    for (int dutyCycle = 0; dutyCycle <= 1023; dutyCycle += 32) {
        ledcWrite(PWM_CHANNEL, dutyCycle);
        delay(1000);
    }

    TEST_MESSAGE("-> starting ramp 100% to 0% (fast)");
    for (int dutyCycle = 1023; dutyCycle >= 0; dutyCycle -= 32) {
        ledcWrite(PWM_CHANNEL, dutyCycle);
        delay(500);
    }

    TEST_MESSAGE("-> starting ramp 100% to 0% (slow)");
    for (int dutyCycle = 1023; dutyCycle >= 0; dutyCycle -= 32) {
        ledcWrite(PWM_CHANNEL, dutyCycle);
        delay(50);
    }
    TEST_MESSAGE("-> 10x quick ramps");
    for (int i = 0; i < 500; i++) {

        for (int dutyCycle = 1023; dutyCycle >= 0; dutyCycle -= 32) {
            ledcWrite(PWM_CHANNEL, dutyCycle);
            delay(10);
        }
    }

    ledcWrite(PWM_CHANNEL, 0);

    TEST_MESSAGE("VISUAL CHECK: did the screen dim? (and flicker?)");
    TEST_ASSERT_TRUE(true);
}

// TEST: Scanning i2c and return devices found
void test_scan_i2c(void) {
    Serial.println("[TEST] testing BQ25895");
    byte error, address;
    int nDevices;

    I2C0_Bus.begin(I2C0_SDA_PIN, I2C0_SCL_PIN);
    nDevices = 0;

    // search all 7-bit addresses
    for (address = 1; address < 127; address++) {
        I2C0_Bus.beginTransmission(address);
        // 0=ACK
        error = I2C0_Bus.endTransmission(true);
        if (error == 0) {
            Serial.printf("[FOUND] i2c device at 0x%02X\n", address);
            nDevices++;
        }
        // 2=NACK; 3=TIMEOUT
    }

    if (nDevices == 0) {
        Serial.println("[ERROR] no i2c device found");
        TEST_FAIL_MESSAGE("No i2c device found on I2C0");
    } else {
        Serial.printf("  %d Found i2c devices \n", nDevices);
        TEST_ASSERT_TRUE(true);
    }
}

void test_charging_ic(void) {
    Serial.println("[TEST] testing BQ25895");

    // charging IC BQ25895 is at 0x6A
    byte error;

    I2C0_Bus.begin(I2C0_SDA_PIN, I2C0_SCL_PIN);

    // confirm part number
    uint8_t val = read_bq_reg(0x14);
    uint8_t part_number = (val >> 3) & 0x07;
    Serial.printf("REG14 Raw: 0x%02X -> Part Number: %d\n", val, part_number);

    if (part_number == 7) {
        Serial.println("[SUCCESS] Chip identified as BQ25895");
    } else {
        Serial.println("[WARNING] Chip ID does not match standard BQ25895 (Expected: 7)");
    }
}

// TODO: should be moved into a common lib later
void set_charging_ic(void) {
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

void test_screen(void) {
    Serial.println("[TEST] E-Paper GDEY029T94 (BW)");
    pinMode(18, OUTPUT);
    digitalWrite(18, HIGH);

    pinMode(EPD_LED_PIN, OUTPUT);
    digitalWrite(EPD_LED_PIN, HIGH);

    SPI.end();
    SPI.begin(SPI_SCLK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN, EPD_CS_PIN);

    display.init(115200, true, 20, false);

    display.setRotation(1);
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_BLACK);

    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);

        display.drawRect(5, 5, 286, 118, GxEPD_BLACK);

        display.setCursor(10, 30);
        display.print("Model: T94-FL03");

        display.setCursor(10, 60);
        display.print("Mono: OK");

    } while (display.nextPage());

    delay(2000);

    display.powerOff();
    digitalWrite(EPD_LED_PIN, LOW);

    Serial.println("[SUCCESS] Screen updated");
    TEST_ASSERT_TRUE(true);
}

void test_graphics_checkerboard(void) {
    Serial.println("[TEST] Graphics: Checkerboard");
    pinMode(18, OUTPUT);
    digitalWrite(18, HIGH);

    pinMode(EPD_LED_PIN, OUTPUT);
    digitalWrite(EPD_LED_PIN, HIGH);

    SPI.end();
    SPI.begin(SPI_SCLK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN, EPD_CS_PIN);

    display.init(115200, true, 20, false);

    display.setRotation(1);
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_BLACK);

    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);

        int boxSize = (rand() % 19) + (rand() % 16);
        for (int y = 0; y < display.height(); y += boxSize) {
            for (int x = 0; x < display.width(); x += boxSize) {
                if ((x / boxSize + y / boxSize) % 2 == 0) {
                    display.fillRect(x, y, boxSize, boxSize, GxEPD_BLACK);
                }
            }
        }

        display.fillRect(10, 10, 200, 60, GxEPD_WHITE);
        display.drawRect(10, 10, 200, 60, GxEPD_BLACK);
        display.setCursor(20, 30);
        display.print("Inspected by C.T.");

    } while (display.nextPage());
    delay(4000);

    display.powerOff();
    TEST_ASSERT_TRUE(true);
}

void test_tropicsquare(void) {
    Serial.println("[TEST] Tropic Square SE");
    pinMode(TR01_CS_PIN, OUTPUT);
    digitalWrite(TR01_CS_PIN, HIGH);
    pinMode(EPD_CS_PIN, OUTPUT);
    digitalWrite(EPD_CS_PIN, HIGH);
    SPI.end();

    SPI.begin(SPI_SCLK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN, TR01_CS_PIN);
    SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
    digitalWrite(TR01_CS_PIN, LOW);
    delayMicroseconds(150);
    uint8_t tx = 0x00;
    uint8_t rx = SPI.transfer(tx);
    digitalWrite(TR01_CS_PIN, HIGH);
    SPI.endTransaction();

    Serial.printf("[SPI] Sent: 0x%02X -> Received: 0x%02X\n", tx, rx);
    if (rx != 0xFF) {
        Serial.println("[SUCCESS] Received valid data");
    } else {
        Serial.println("[INFO] Received 0xFF (Bus might be idle)");
    }

    TEST_ASSERT_TRUE(true);
}

void test_buttons(void) {}

void setup() {

    UNITY_BEGIN();
    setup_usb_serial();
    bq25895_init();

    RUN_TEST(test_scan_i2c);
    RUN_TEST(test_charging_ic);
    RUN_TEST(set_charging_ic);
    RUN_TEST(test_tropicsquare);

    RUN_TEST(test_screen);
    RUN_TEST(test_serial_communication);
    RUN_TEST(test_gpio_led_output);
    RUN_TEST(test_EPD_light);
    RUN_TEST(test_EPD_pwm);
    RUN_TEST(test_graphics_checkerboard);

    UNITY_END();
}

void loop() {
    if (UNITY_END()) {
        delay(1000);
    }
}

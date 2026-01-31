/**
 * TCA9535 I/O Expander - 12-Key Keypad HAL Implementation
 * Interrupt-driven with FreeRTOS task and circular buffer
 */

#include "cdc_hal/IKeypad.h"
#include "cdc_hal/II2cBus.h"
#include "cdc_hal/hw_config.h"
#include "cdc_log.h"
#include "esp_attr.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char* TAG = "Keypad";

namespace cdc::hal {

// TCA9535 register addresses
static constexpr uint8_t REG_INPUT_0    = 0x00;
static constexpr uint8_t REG_INPUT_1    = 0x01;
static constexpr uint8_t REG_OUTPUT_0   = 0x02;
static constexpr uint8_t REG_OUTPUT_1   = 0x03;
static constexpr uint8_t REG_POLARITY_0 = 0x04;
static constexpr uint8_t REG_POLARITY_1 = 0x05;
static constexpr uint8_t REG_CONFIG_0   = 0x06;
static constexpr uint8_t REG_CONFIG_1   = 0x07;

// Task configuration
static constexpr uint32_t TASK_STACK_SIZE = 4096;
static constexpr UBaseType_t TASK_PRIORITY = 5;
static constexpr uint32_t POLL_TIMEOUT_MS = 50;
static constexpr uint32_t DEBOUNCE_MS = 10;

// Key buffer configuration
static constexpr size_t KEY_BUFFER_SIZE = 16;

/**
 * Convert raw 16-bit input to Key enum
 * Keys are active-low, mapped to bits 0-11
 */
static Key rawToKey(uint16_t raw) {
    switch (raw & 0x0FFF) {
        case 0b111111111110: return Key::KEY_0;
        case 0b111111111101: return Key::KEY_1;
        case 0b111111111011: return Key::KEY_2;
        case 0b111111110111: return Key::KEY_3;
        case 0b111111101111: return Key::KEY_4;
        case 0b111111011111: return Key::KEY_5;
        case 0b111110111111: return Key::KEY_6;
        case 0b111101111111: return Key::KEY_7;
        case 0b111011111111: return Key::KEY_8;
        case 0b110111111111: return Key::KEY_9;
        case 0b011111111111: return Key::KEY_NO;    // Cancel/N
        case 0b101111111111: return Key::KEY_YES;   // OK/Y
        default: return Key::KEY_NONE;
    }
}

/**
 * Convert Key to expected raw mask (12-bit)
 */
static uint16_t keyToMask(Key key) {
    switch (key) {
        case Key::KEY_0: return 0b111111111110;
        case Key::KEY_1: return 0b111111111101;
        case Key::KEY_2: return 0b111111111011;
        case Key::KEY_3: return 0b111111110111;
        case Key::KEY_4: return 0b111111101111;
        case Key::KEY_5: return 0b111111011111;
        case Key::KEY_6: return 0b111110111111;
        case Key::KEY_7: return 0b111101111111;
        case Key::KEY_8: return 0b111011111111;
        case Key::KEY_9: return 0b110111111111;
        case Key::KEY_NO: return 0b011111111111;
        case Key::KEY_YES: return 0b101111111111;
        default: return 0xFFFF;
    }
}

/**
 * Concrete TCA9535 keypad implementation
 */
class TCA9535Keypad : public IKeypad {
public:
    TCA9535Keypad() = default;

    // IService implementation
    bool init() override;
    bool start() override;
    void stop() override;
    core::ServiceState getState() const override { return state_; }
    const char* getName() const override { return "keypad"; }

    // IKeypad implementation
    void poll() override {}  // Handled by task
    bool isKeyPressed(Key key) const override;
    Key getNextKey() override;
    bool hasKey() const override;
    bool anyKeyDown() const override;
    void setCallback(KeyCallback callback) override { callback_ = callback; }
    void setLongPressEnabled(bool enabled, uint32_t thresholdMs) override;
    void setLongPressCallback(LongPressCallback callback) override { longPressCallback_ = callback; }
    void prepareForSleep() override;
    void recoverFromSleep() override;
    void clearBuffer() override;

private:
    uint16_t readInputs();
    void bufferAddKey(Key key);
    Key bufferGetKey();
    static void taskFunc(void* arg);
    static void IRAM_ATTR isrHandler(void* arg);

    core::ServiceState state_ = core::ServiceState::UNINITIALIZED;

    // I2C device
    II2cBus* bus_ = nullptr;
    I2cDeviceHandle device_ = nullptr;

    // Key buffer (circular) - protected by bufferMux_
    Key keyBuffer_[KEY_BUFFER_SIZE] = {};
    uint8_t bufferHead_ = 0;
    uint8_t bufferTail_ = 0;
    mutable portMUX_TYPE bufferMux_ = portMUX_INITIALIZER_UNLOCKED;

    // Task handles
    SemaphoreHandle_t semaphore_ = nullptr;
    TaskHandle_t taskHandle_ = nullptr;

    // State
    uint16_t lastRawState_ = 0xFFFF;
    volatile bool inSleepMode_ = false;

    // Callbacks
    KeyCallback callback_ = nullptr;
    LongPressCallback longPressCallback_ = nullptr;
    bool longPressEnabled_ = false;
    uint32_t longPressThresholdMs_ = 800;

    // Long-press tracking
    Key pressedKey_ = Key::KEY_NONE;
    uint32_t pressStartTime_ = 0;
    bool longPressFired_ = false;
};

bool TCA9535Keypad::init() {
    if (state_ != core::ServiceState::UNINITIALIZED) {
        return state_ == core::ServiceState::INITIALIZED ||
               state_ == core::ServiceState::STARTED;
    }

    // Get I2C bus
    bus_ = getI2cBus0();
    if (!bus_ || bus_->getState() != core::ServiceState::INITIALIZED) {
        LOG_E(TAG, "I2C bus not initialized");
        state_ = core::ServiceState::ERROR;
        return false;
    }

    // Add TCA9535 device
    if (bus_->addDevice(EXPANDER_ADDR, &device_) != ESP_OK) {
        LOG_E(TAG, "Failed to add TCA9535 device");
        state_ = core::ServiceState::ERROR;
        return false;
    }

    // Configure TCA9535
    uint8_t allHigh = 0xFF;
    uint8_t noInvert = 0x00;
    uint8_t allInputs = 0xFF;

    // Set output registers high (for proper pull-up reading)
    bus_->writeReg(device_, REG_OUTPUT_0, &allHigh, 1);
    bus_->writeReg(device_, REG_OUTPUT_1, &allHigh, 1);

    // No polarity inversion
    bus_->writeReg(device_, REG_POLARITY_0, &noInvert, 1);
    bus_->writeReg(device_, REG_POLARITY_1, &noInvert, 1);

    // Configure all pins as inputs
    if (bus_->writeReg(device_, REG_CONFIG_0, &allInputs, 1) != ESP_OK ||
        bus_->writeReg(device_, REG_CONFIG_1, &allInputs, 1) != ESP_OK) {
        LOG_E(TAG, "Failed to configure TCA9535");
        state_ = core::ServiceState::ERROR;
        return false;
    }

    // Read initial state to clear any pending interrupt
    lastRawState_ = readInputs();
    LOG_I(TAG, "Initial state: 0x%04X", lastRawState_);

    // Create semaphore
    semaphore_ = xSemaphoreCreateBinary();
    if (!semaphore_) {
        LOG_E(TAG, "Failed to create semaphore");
        state_ = core::ServiceState::ERROR;
        return false;
    }

    // Create task
    BaseType_t ret = xTaskCreate(taskFunc, "keypad", TASK_STACK_SIZE,
                                  this, TASK_PRIORITY, &taskHandle_);
    if (ret != pdPASS) {
        LOG_E(TAG, "Failed to create task");
        vSemaphoreDelete(semaphore_);
        semaphore_ = nullptr;
        state_ = core::ServiceState::ERROR;
        return false;
    }

    // Configure interrupt pin
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << EXP_IRQ_PIN);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    gpio_config(&io_conf);

    // Install ISR service (may already be installed by another driver)
    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        LOG_E(TAG, "Failed to install ISR service: %s", esp_err_to_name(err));
        vSemaphoreDelete(semaphore_);
        semaphore_ = nullptr;
        state_ = core::ServiceState::ERROR;
        return false;
    }
    gpio_isr_handler_add(EXP_IRQ_PIN, isrHandler, this);

    state_ = core::ServiceState::INITIALIZED;
    LOG_I(TAG, "TCA9535 keypad initialized (IRQ=GPIO%d)", EXP_IRQ_PIN);
    return true;
}

bool TCA9535Keypad::start() {
    if (state_ == core::ServiceState::INITIALIZED ||
        state_ == core::ServiceState::STOPPED) {
        state_ = core::ServiceState::STARTED;
        return true;
    }
    return state_ == core::ServiceState::STARTED;
}

void TCA9535Keypad::stop() {
    if (state_ == core::ServiceState::STARTED) {
        state_ = core::ServiceState::STOPPED;
    }
}

uint16_t TCA9535Keypad::readInputs() {
    if (!device_) return 0xFFFF;

    uint8_t lo = 0xFF, hi = 0xFF;
    if (bus_->readReg(device_, REG_INPUT_0, &lo, 1) != ESP_OK) return 0xFFFF;
    if (bus_->readReg(device_, REG_INPUT_1, &hi, 1) != ESP_OK) return 0xFFFF;

    return (uint16_t)((hi << 8) | lo);
}

bool TCA9535Keypad::isKeyPressed(Key key) const {
    uint16_t mask = keyToMask(key);
    if (mask == 0xFFFF) return false;

    uint16_t current = const_cast<TCA9535Keypad*>(this)->readInputs();
    return (current & 0x0FFF) == mask;
}

Key TCA9535Keypad::getNextKey() {
    return bufferGetKey();
}

bool TCA9535Keypad::hasKey() const {
    portENTER_CRITICAL(&bufferMux_);
    bool hasKey = bufferHead_ != bufferTail_;
    portEXIT_CRITICAL(&bufferMux_);
    return hasKey;
}

bool TCA9535Keypad::anyKeyDown() const {
    uint16_t current = const_cast<TCA9535Keypad*>(this)->readInputs();
    // 0x0FFF = all 12 keys released (bits 0-11 high, bits 12-15 don't care)
    return (current & 0x0FFF) != 0x0FFF;
}

void TCA9535Keypad::setLongPressEnabled(bool enabled, uint32_t thresholdMs) {
    longPressEnabled_ = enabled;
    longPressThresholdMs_ = thresholdMs;
}

void TCA9535Keypad::bufferAddKey(Key key) {
    if (key == Key::KEY_NONE) return;

    portENTER_CRITICAL(&bufferMux_);
    uint8_t nextHead = (bufferHead_ + 1) % KEY_BUFFER_SIZE;
    if (nextHead != bufferTail_) {
        keyBuffer_[bufferHead_] = key;
        bufferHead_ = nextHead;
    }
    portEXIT_CRITICAL(&bufferMux_);
}

Key TCA9535Keypad::bufferGetKey() {
    portENTER_CRITICAL(&bufferMux_);
    if (bufferHead_ == bufferTail_) {
        portEXIT_CRITICAL(&bufferMux_);
        return Key::KEY_NONE;
    }

    Key key = keyBuffer_[bufferTail_];
    bufferTail_ = (bufferTail_ + 1) % KEY_BUFFER_SIZE;
    portEXIT_CRITICAL(&bufferMux_);
    return key;
}

void IRAM_ATTR TCA9535Keypad::isrHandler(void* arg) {
    auto* self = static_cast<TCA9535Keypad*>(arg);
    if (self->inSleepMode_) return;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(self->semaphore_, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void TCA9535Keypad::taskFunc(void* arg) {
    auto* self = static_cast<TCA9535Keypad*>(arg);
    LOG_I(TAG, "Keypad task started");

    while (true) {
        // Wait for IRQ or timeout (poll fallback)
        xSemaphoreTake(self->semaphore_, pdMS_TO_TICKS(POLL_TIMEOUT_MS));

        // Small debounce delay
        vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_MS));

        // Read current state
        uint16_t raw = self->readInputs();

        if (raw != self->lastRawState_) {
            Key key = rawToKey(raw);

            // Key press detection
            if (key != Key::KEY_NONE) {
                self->bufferAddKey(key);

                if (self->callback_) {
                    self->callback_(key, true);
                }

                // Start long-press tracking
                if (self->longPressEnabled_) {
                    self->pressedKey_ = key;
                    self->pressStartTime_ = xTaskGetTickCount() * portTICK_PERIOD_MS;
                    self->longPressFired_ = false;
                }
            } else {
                // Key release
                if (self->callback_ && self->pressedKey_ != Key::KEY_NONE) {
                    self->callback_(self->pressedKey_, false);
                }
                self->pressedKey_ = Key::KEY_NONE;
            }

            self->lastRawState_ = raw;
        }

        // Long-press check
        if (self->longPressEnabled_ && self->pressedKey_ != Key::KEY_NONE && !self->longPressFired_) {
            uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if (now - self->pressStartTime_ >= self->longPressThresholdMs_) {
                self->longPressFired_ = true;
                if (self->longPressCallback_) {
                    self->longPressCallback_(self->pressedKey_);
                }
            }
        }
    }
}

void TCA9535Keypad::prepareForSleep() {
    LOG_D(TAG, "Preparing keypad for sleep...");

    // 1. Disable ISR processing flag (prevents ISR from doing anything)
    inSleepMode_ = true;

    // 2. Disable GPIO interrupt at hardware level
    gpio_intr_disable(EXP_IRQ_PIN);
}

void TCA9535Keypad::recoverFromSleep() {
    LOG_D(TAG, "Recovering keypad after sleep...");

    // 1. Disable GPIO interrupt (may already be disabled, but be safe)
    gpio_intr_disable(EXP_IRQ_PIN);

    // 2. Wait for all keys to be released (level-triggered wakeup keeps pin LOW)
    int timeout = 200;  // 2 seconds max (200 * 10ms)
    while (anyKeyDown() && timeout > 0) {
        vTaskDelay(pdMS_TO_TICKS(10));
        timeout--;
    }
    vTaskDelay(pdMS_TO_TICKS(30));  // Extra debounce

    // 3. Clear key buffer (keys from wakeup press)
    clearBuffer();

    // 4. Reset last raw state to current state
    lastRawState_ = readInputs();

    // 5. Re-enable ISR processing flag
    inSleepMode_ = false;

    // 6. Restore edge-triggered interrupt type (wakeup used level-triggered)
    gpio_set_intr_type(EXP_IRQ_PIN, GPIO_INTR_NEGEDGE);

    // 7. Re-enable GPIO interrupt
    gpio_intr_enable(EXP_IRQ_PIN);

    LOG_D(TAG, "Keypad recovered, state: 0x%04X", lastRawState_);
}

void TCA9535Keypad::clearBuffer() {
    portENTER_CRITICAL(&bufferMux_);
    bufferHead_ = 0;
    bufferTail_ = 0;
    portEXIT_CRITICAL(&bufferMux_);
}

// Singleton instance
static TCA9535Keypad g_keypad;

// Factory function
IKeypad* getKeypadInstance() {
    return &g_keypad;
}

} // namespace cdc::hal

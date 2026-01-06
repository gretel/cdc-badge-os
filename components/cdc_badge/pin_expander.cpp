// TCA9535 I/O Expander - 12-Key Keypad Driver
// Handles interrupt-driven keypad input with a small FIFO buffer.
// A dedicated FreeRTOS task ensures keypresses are captured immediately,
// even during slow E-Paper display updates.

#include "pin_expander.h"
#include "i2c_bus.h"
#include "hw_config.h"
#include "cdc_log.h"
#include "cdc_gpio.h"
#include "driver/gpio.h"
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// TCA9535 register addresses
static constexpr uint8_t REG_INPUT_0    = 0x00;  // Input port 0 (P0.0-P0.7)
static constexpr uint8_t REG_INPUT_1    = 0x01;  // Input port 1 (P1.0-P1.7)
static constexpr uint8_t REG_OUTPUT_0   = 0x02;  // Output port 0
static constexpr uint8_t REG_OUTPUT_1   = 0x03;  // Output port 1
static constexpr uint8_t REG_POLARITY_0 = 0x04;  // Polarity inversion port 0
static constexpr uint8_t REG_POLARITY_1 = 0x05;  // Polarity inversion port 1
static constexpr uint8_t REG_CONFIG_0   = 0x06;  // Configuration port 0 (1=input)
static constexpr uint8_t REG_CONFIG_1   = 0x07;  // Configuration port 1 (1=input)

// Key buffer for async operation
#define KEY_BUFFER_SIZE 16
static volatile char key_buffer[KEY_BUFFER_SIZE];
static volatile uint8_t key_buffer_head = 0;
static volatile uint8_t key_buffer_tail = 0;

// Keypad task handles
static SemaphoreHandle_t keypad_sem = nullptr;
static TaskHandle_t keypad_task_handle = nullptr;
#define KEYPAD_TASK_STACK_SIZE 2048
#define KEYPAD_TASK_PRIORITY 10  // Higher than display task

// Device handle and state
static i2c_master_dev_handle_t expander_dev = nullptr;
static uint16_t last_raw_state = 0xFFFF;

// Sleep mode flag - ISR does nothing when true
static volatile bool g_in_sleep_mode = false;

// ISR handler - gives semaphore to wake keypad task
static void IRAM_ATTR expander_isr_handler(void *arg) {
    (void)arg;
    // Skip if in sleep mode (will be handled after wakeup)
    if (g_in_sleep_mode) return;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(keypad_sem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Disable ISR (call before light sleep)
void pin_expander_disable_isr(void) {
    g_in_sleep_mode = true;
}

// Enable ISR (call after light sleep wakeup)
void pin_expander_enable_isr(void) {
    g_in_sleep_mode = false;
}

// Convert raw 16-bit input reading to key character
// Hardware uses 12 keys mapped to bits 0-11 of TCA9535
// Mask to 12 bits since upper bits may vary
static char raw_to_key(uint16_t raw) {
    switch (raw & 0x0FFF) {
        case 0b111111111110: return '0';
        case 0b111111111101: return '1';
        case 0b111111111011: return '2';
        case 0b111111110111: return '3';
        case 0b111111101111: return '4';
        case 0b111111011111: return '5';
        case 0b111110111111: return '6';
        case 0b111101111111: return '7';
        case 0b111011111111: return '8';
        case 0b110111111111: return '9';
        case 0b011111111111: return 'N';
        case 0b101111111111: return 'Y';
        default: return 'x';
    }
}

// Convert key character to expected raw mask (12-bit)
static uint16_t key_to_mask(char key) {
    switch (key) {
        case '0': return 0b111111111110;
        case '1': return 0b111111111101;
        case '2': return 0b111111111011;
        case '3': return 0b111111110111;
        case '4': return 0b111111101111;
        case '5': return 0b111111011111;
        case '6': return 0b111110111111;
        case '7': return 0b111101111111;
        case '8': return 0b111011111111;
        case '9': return 0b110111111111;
        case 'N': return 0b011111111111;
        case 'Y': return 0b101111111111;
        default:  return 0xFFFF;
    }
}

// Add key to circular buffer
static void buffer_add_key(char key) {
    if (key == 'x') return;

    uint8_t next_head = (key_buffer_head + 1) % KEY_BUFFER_SIZE;
    if (next_head != key_buffer_tail) {  // Not full
        key_buffer[key_buffer_head] = key;
        key_buffer_head = next_head;
    }
}

// Get key from circular buffer
static char buffer_get_key(void) {
    if (key_buffer_head == key_buffer_tail) return 'x';  // Empty

    char key = key_buffer[key_buffer_tail];
    key_buffer_tail = (key_buffer_tail + 1) % KEY_BUFFER_SIZE;
    return key;
}

// Read 16-bit input state from expander
static uint16_t expander_read_inputs(void) {
    if (!expander_dev) return 0xFFFF;

    uint8_t lo = 0xFF;
    uint8_t hi = 0xFF;

    if (i2c_read_reg(expander_dev, REG_INPUT_0, &lo, 1) != ESP_OK) return 0xFFFF;
    if (i2c_read_reg(expander_dev, REG_INPUT_1, &hi, 1) != ESP_OK) return 0xFFFF;

    return (uint16_t)((hi << 8) | lo);
}

// Keypad task - runs at high priority to capture keys immediately
static void keypad_task(void *arg) {
    (void)arg;
    LOG_I("KEYPAD", "Keypad task started");

    while (true) {
        // Wait for IRQ or timeout (poll every 50ms as fallback)
        xSemaphoreTake(keypad_sem, pdMS_TO_TICKS(50));

        // Small debounce delay
        vTaskDelay(pdMS_TO_TICKS(10));

        // Read current state
        uint16_t raw = expander_read_inputs();

        // Only register key press (not release)
        if (raw != last_raw_state) {
            char key = raw_to_key(raw);
            if (key != 'x') {
                buffer_add_key(key);
            }
            last_raw_state = raw;
        }
    }
}

bool pin_expander_init(void) {
    LOG_I("KEYPAD", "pin_expander_init() starting...");

    // Check if I2C bus is valid
    if (!I2C0_Bus) {
        LOG_E("KEYPAD", "I2C0_Bus is NULL!");
        return false;
    }
    LOG_I("KEYPAD", "I2C0_Bus OK, adding TCA9535 at 0x%02X...", EXPANDER_ADDR);

    // Add TCA9535 to I2C bus
    esp_err_t err = i2c_bus_add_device(I2C0_Bus, EXPANDER_ADDR, &expander_dev);
    if (err != ESP_OK) {
        LOG_E("KEYPAD", "Failed to add TCA9535 (err=%d)", err);
        return false;
    }
    LOG_I("KEYPAD", "TCA9535 device added OK");

    // Set output registers high (needed for proper input reading with external pull-ups)
    uint8_t all_high = 0xFF;
    i2c_write_reg(expander_dev, REG_OUTPUT_0, &all_high, 1);
    i2c_write_reg(expander_dev, REG_OUTPUT_1, &all_high, 1);

    // No polarity inversion (0 = normal)
    uint8_t no_invert = 0x00;
    i2c_write_reg(expander_dev, REG_POLARITY_0, &no_invert, 1);
    i2c_write_reg(expander_dev, REG_POLARITY_1, &no_invert, 1);

    // Configure all pins as inputs (1 = input in config register)
    uint8_t all_inputs = 0xFF;
    if (i2c_write_reg(expander_dev, REG_CONFIG_0, &all_inputs, 1) != ESP_OK ||
        i2c_write_reg(expander_dev, REG_CONFIG_1, &all_inputs, 1) != ESP_OK) {
        LOG_E("KEYPAD", "Failed to configure TCA9535 as inputs");
        return false;
    }

    // Read initial state to clear any pending interrupt
    last_raw_state = expander_read_inputs();
    LOG_I("KEYPAD", "Initial raw state: 0x%04X (expected 0x0FFF if no key pressed)", last_raw_state);

    // Create semaphore for ISR -> task communication
    keypad_sem = xSemaphoreCreateBinary();
    if (!keypad_sem) {
        LOG_E("KEYPAD", "Failed to create semaphore");
        return false;
    }

    // Create keypad task (high priority to capture keys during display updates)
    BaseType_t ret = xTaskCreate(keypad_task, "keypad", KEYPAD_TASK_STACK_SIZE,
                                  NULL, KEYPAD_TASK_PRIORITY, &keypad_task_handle);
    if (ret != pdPASS) {
        LOG_E("KEYPAD", "Failed to create keypad task");
        vSemaphoreDelete(keypad_sem);
        keypad_sem = nullptr;
        return false;
    }

    // Configure expander interrupt pin (active-low, open-drain)
    cdc_gpio_config_input(EXP_IRQ_PIN, true, false);
    cdc_gpio_install_isr_service_once(0);
    gpio_set_intr_type(EXP_IRQ_PIN, GPIO_INTR_NEGEDGE);
    gpio_isr_handler_add(EXP_IRQ_PIN, expander_isr_handler, nullptr);

    LOG_I("KEYPAD", "Keypad initialized (12 keys, IRQ=GPIO%d, async task)", EXP_IRQ_PIN);
    return true;
}

void pin_expander_poll(void) {
    // No-op: keypad task handles polling asynchronously
    // Kept for API compatibility
}

char pin_expander_get_key(void) {
    // Just return next key from buffer (filled by keypad task)
    return buffer_get_key();
}

bool pin_expander_is_key_down(char key) {
    uint16_t mask = key_to_mask(key);
    if (mask == 0xFFFF) return false;

    // Read current state without modifying last_raw_state (avoid race with keypad task)
    uint16_t current = expander_read_inputs();
    // Mask to 12 bits since upper bits (12-15) may vary
    return (current & 0x0FFF) == mask;
}

bool pin_expander_any_key_down(void) {
    // Read current state without modifying last_raw_state (avoid race with keypad task)
    uint16_t current = expander_read_inputs();
    // 0x0FFF = all 12 keys released (bits 0-11 high, bits 12-15 don't care)
    return (current & 0x0FFF) != 0x0FFF;
}

bool pin_expander_has_key(void) {
    return key_buffer_head != key_buffer_tail;
}

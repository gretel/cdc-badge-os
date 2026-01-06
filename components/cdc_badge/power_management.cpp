// BQ25895 Power Management for CDC Badge
// Applies safe defaults on every boot (charger has no persistent storage).
// Supports fast charging up to 1000mA for the 1200mAh LiPo battery.

#include "power_management.h"
#include "pin_expander.h"
#include "i2c_bus.h"
#include "hw_config.h"
#include "cdc_log.h"
#include "cdc_time.h"
#include "cdc_gpio.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// BQ25895 Register Map
static constexpr uint8_t BQ_REG_INPUT_CTRL   = 0x00;  // Input source control
static constexpr uint8_t BQ_REG_ADC_CTRL     = 0x02;  // ADC control
static constexpr uint8_t BQ_REG_CHG_CTRL     = 0x03;  // Charge control (SYS_MIN, OTG)
static constexpr uint8_t BQ_REG_FAST_CHG     = 0x04;  // Fast charge current
static constexpr uint8_t BQ_REG_TIMER        = 0x07;  // Charge timer
static constexpr uint8_t BQ_REG_MISC         = 0x09;  // Misc (BATFET_DIS)
static constexpr uint8_t BQ_REG_SYS_STATUS   = 0x0B;  // System status
static constexpr uint8_t BQ_REG_FAULT        = 0x0C;  // Fault status
static constexpr uint8_t BQ_REG_BATV         = 0x0E;  // Battery voltage ADC
static constexpr uint8_t BQ_REG_SYSV         = 0x0F;  // System voltage ADC
static constexpr uint8_t BQ_REG_TS           = 0x10;  // TS ADC
static constexpr uint8_t BQ_REG_VBUS         = 0x11;  // VBUS voltage ADC
static constexpr uint8_t BQ_REG_ICHG         = 0x12;  // Charge current ADC
static constexpr uint8_t BQ_REG_VINDPM       = 0x13;  // VINDPM threshold
static constexpr uint8_t BQ_REG_VENDOR       = 0x14;  // Vendor/Part info

// Charge current settings
static constexpr uint8_t  BQ_ICHG_STEP_MA = 64;       // REG04[6:0] step size
static constexpr uint16_t BQ_SYS_MIN_MV   = 3300;     // Minimum system voltage

// Long-press duration for shipping mode
static constexpr uint32_t POWER_BUTTON_LONG_PRESS_MS = 3000;

// Device handle
static i2c_master_dev_handle_t bq_dev = nullptr;

// IRQ flags (set in ISR, handled in process())
static volatile bool charger_irq_pending = false;
static volatile bool power_button_irq = false;

// Power button long-press tracking
static bool power_button_held = false;
static uint32_t power_button_hold_start_ms = 0;

// Current charge setting
static uint16_t current_charge_ma = CHARGE_CURRENT_SLOW;
static bool fast_charge_enabled = false;


// ISR handlers
static void IRAM_ATTR charger_isr(void *arg) {
    (void)arg;
    charger_irq_pending = true;
}

static void IRAM_ATTR power_button_isr(void *arg) {
    (void)arg;
    power_button_irq = true;
}

// Read/write helpers
// Returns ESP_OK on success, value in *out_val
static esp_err_t read_reg(uint8_t reg, uint8_t *out_val) {
    if (!bq_dev || !out_val) return ESP_ERR_INVALID_ARG;
    return i2c_read_reg(bq_dev, reg, out_val, 1);
}

static bool write_reg(uint8_t reg, uint8_t val) {
    if (!bq_dev) return false;
    return i2c_write_reg(bq_dev, reg, &val, 1) == ESP_OK;
}

// Update specific bits in a register (read-modify-write)
static bool update_reg_bits(uint8_t reg, uint8_t mask, uint8_t val, const char *label) {
    uint8_t current = 0;
    if (read_reg(reg, &current) != ESP_OK) {
        LOG_E("POWER", "Read failed: %s", label);
        return false;
    }

    uint8_t new_val = (current & ~mask) | (val & mask);
    if (new_val == current) {
        LOG_D("POWER", "%s already set (0x%02X)", label, current);
        return true;
    }

    if (!write_reg(reg, new_val)) {
        LOG_E("POWER", "Write failed: %s", label);
        return false;
    }

    LOG_D("POWER", "%s: 0x%02X -> 0x%02X", label, current, new_val);
    return true;
}

// Read charger status registers (clears latched fault flags)
static void read_charger_status(void) {
    uint8_t reg0b = 0, reg0c = 0;
    uint8_t chrg_stat = 0;

    if (read_reg(BQ_REG_SYS_STATUS, &reg0b) == ESP_OK) {
        uint8_t vbus_stat = (reg0b >> 5) & 0x07;
        chrg_stat = (reg0b >> 3) & 0x03;
        bool pg_stat = (reg0b >> 2) & 0x01;

        // Log charge status with human-readable text
        const char* chrg_text[] = {"Not charging", "Pre-charge", "Fast charging", "Charge done"};
        LOG_D("POWER", "Status: VBUS=%d CHRG=%s PG=%d", vbus_stat, chrg_text[chrg_stat], pg_stat);
    }

    if (read_reg(BQ_REG_FAULT, &reg0c) == ESP_OK && reg0c != 0x00) {
        // Watchdog fault (0x80) when battery is full is expected behavior, not a real fault
        if (reg0c == 0x80 && chrg_stat == 3) {
            LOG_I("POWER", "Battery full");
        } else if (reg0c == 0x80) {
            // Watchdog expired but battery not full - kick it to resume charging
            LOG_D("POWER", "Watchdog expired, resetting");
            update_reg_bits(BQ_REG_CHG_CTRL, (1 << 6), (1 << 6), "WDT reset");
        } else {
            // Real fault
            LOG_W("POWER", "Fault: 0x%02X", reg0c);
        }
    }
}

bool power_management_init(void) {
    LOG_I("POWER", "power_management_init()");

    // Add BQ25895 to I2C bus
    if (i2c_bus_add_device(I2C0_Bus, BQ25895_ADDR, &bq_dev) != ESP_OK) {
        LOG_E("POWER", "Failed to add BQ25895 to I2C0");
        return false;
    }

    // Verify chip ID (REG14[5:3] should be 0b111 for BQ25895)
    uint8_t vendor = 0;
    if (read_reg(BQ_REG_VENDOR, &vendor) != ESP_OK) {
        LOG_E("POWER", "Failed to read BQ25895 vendor register");
        return false;
    }
    uint8_t part_number = (vendor >> 3) & 0x07;
    if (part_number != 7) {
        LOG_E("POWER", "BQ25895 not detected (got part=%d)", part_number);
        return false;
    }
    LOG_I("POWER", "BQ25895 detected (REG14=0x%02X)", vendor);

    // Apply safe defaults (charger has NO persistent storage!)

    // 1) Disable ILIM pin (REG00[6]=0) - use internal limit
    if (!update_reg_bits(BQ_REG_INPUT_CTRL, (1 << 6), 0x00, "ILIM disable")) {
        return false;
    }

    // 2) Set minimum system voltage to 3.3V (REG03[3:1])
    //    Formula: SYS_MIN = 3.0V + code*0.1V -> code = 3 for 3.3V
    uint8_t sys_min_code = (BQ_SYS_MIN_MV - 3000) / 100;
    if (!update_reg_bits(BQ_REG_CHG_CTRL, 0x0E, (uint8_t)(sys_min_code << 1), "SYS_MIN=3.3V")) {
        return false;
    }

    // 3) Disable OTG boost (REG03[5]=0)
    if (!update_reg_bits(BQ_REG_CHG_CTRL, (1 << 5), 0x00, "OTG disable")) {
        return false;
    }

    // 4) Set initial charge current (default: slow charge 512mA)
    if (!power_set_charge_current_ma(CHARGE_CURRENT_SLOW)) {
        return false;
    }

    // 5) Disable USB D+/D- detection (REG02[0]=0)
    //    This prevents the charger from pulling D+/D- during detection.
    //    Trade-off: We can't auto-detect charger type, but USB data works.
    if (!update_reg_bits(BQ_REG_ADC_CTRL, (1 << 0), 0x00, "DPDM disable")) {
        return false;
    }

    // Configure CHG_DSEL as input (active-low during detection)
    cdc_gpio_config_input(CHG_DSEL_PIN, false, false);

    // Configure charger IRQ (active-low, open-drain)
    cdc_gpio_config_input(CHG_IRQ_PIN, true, false);
    cdc_gpio_install_isr_service_once(0);
    gpio_set_intr_type(CHG_IRQ_PIN, GPIO_INTR_NEGEDGE);
    gpio_isr_handler_add(CHG_IRQ_PIN, charger_isr, nullptr);

    // Configure power button IRQ (active-low)
    cdc_gpio_config_input(FLASH_BTN_PIN, true, false);
    gpio_set_intr_type(FLASH_BTN_PIN, GPIO_INTR_NEGEDGE);
    gpio_isr_handler_add(FLASH_BTN_PIN, power_button_isr, nullptr);

    // Read initial status (clears any stale IRQ flags)
    read_charger_status();

    LOG_I("POWER", "Power management initialized");
    return true;
}

void power_management_process(void) {
    uint32_t now = millis();

    // Handle charger IRQ
    if (charger_irq_pending) {
        charger_irq_pending = false;
        LOG_I("POWER", "Charger IRQ received");
        read_charger_status();
    }

    // Poll power button directly (interrupt may not work on all boards)
    static bool last_btn_state = true;  // true = not pressed (active-low)
    bool btn_pressed = (gpio_get_level(FLASH_BTN_PIN) == 0);

    // Detect press edge
    if (btn_pressed && !power_button_held && last_btn_state) {
        power_button_held = true;
        power_button_hold_start_ms = now;
        LOG_I("POWER", "Power button pressed (polled)");
    }
    last_btn_state = !btn_pressed;

    // Also handle interrupt-based detection
    if (power_button_irq) {
        power_button_irq = false;
        if (!power_button_held) {
            power_button_held = true;
            power_button_hold_start_ms = now;
            LOG_I("POWER", "Power button IRQ");
        }
    }

    if (power_button_held) {
        // Check if button was released
        if (!btn_pressed) {
            uint32_t hold_time = now - power_button_hold_start_ms;
            LOG_I("POWER", "Power button released after %lu ms", hold_time);
            power_button_held = false;
            return;
        }

        // Check for long-press timeout
        if (now - power_button_hold_start_ms >= POWER_BUTTON_LONG_PRESS_MS) {
            LOG_W("POWER", "Long-press detected (%lu ms), entering shipping mode",
                  now - power_button_hold_start_ms);
            power_enter_shipping_mode();
            power_button_held = false;
        }
    }
}

bool power_set_charge_current_ma(uint16_t current_ma) {
    // Clamp to valid range
    if (current_ma < CHARGE_CURRENT_MIN) current_ma = CHARGE_CURRENT_MIN;
    if (current_ma > CHARGE_CURRENT_MAX) current_ma = CHARGE_CURRENT_MAX;

    // Calculate register code: ICHG = code * 64mA
    uint8_t code = (uint8_t)(current_ma / BQ_ICHG_STEP_MA);

    // REG04[6:0] = charge current code
    if (!update_reg_bits(BQ_REG_FAST_CHG, 0x7F, code, "ICHG")) {
        return false;
    }

    current_charge_ma = code * BQ_ICHG_STEP_MA;
    LOG_I("POWER", "Charge current set to %dmA (code=%d)", current_charge_ma, code);
    return true;
}

uint16_t power_get_charge_current_ma(void) {
    return current_charge_ma;
}

void power_set_fast_charge(bool fast) {
    fast_charge_enabled = fast;
    power_set_charge_current_ma(fast ? CHARGE_CURRENT_FAST : CHARGE_CURRENT_SLOW);
}

bool power_is_fast_charge(void) {
    return fast_charge_enabled;
}

uint16_t power_get_battery_voltage_mv(void) {
    // Start ADC conversion if not running (REG02[7]=CONV_START)
    uint8_t reg02 = 0;
    if (read_reg(BQ_REG_ADC_CTRL, &reg02) == ESP_OK && !(reg02 & 0x80)) {
        write_reg(BQ_REG_ADC_CTRL, reg02 | 0x80);
        vTaskDelay(pdMS_TO_TICKS(10));  // Wait for ADC conversion
    }

    uint8_t batv = 0;
    if (read_reg(BQ_REG_BATV, &batv) != ESP_OK) {
        return 0;
    }

    // Formula: VBAT = 2304mV + (BATV[6:0] * 20mV)
    uint8_t code = batv & 0x7F;
    return 2304 + (code * 20);
}

uint8_t power_get_battery_percent(void) {
    uint16_t mv = power_get_battery_voltage_mv();
    if (mv == 0) return 0;

    // Linear approximation: 3200mV=0%, 4200mV=100%
    if (mv <= 3200) return 0;
    if (mv >= 4200) return 100;

    return (uint8_t)(((uint32_t)(mv - 3200) * 100) / 1000);
}

bool power_is_usb_connected(void) {
    return power_get_vbus_status() != VBUS_STATUS_NONE;
}

vbus_status_t power_get_vbus_status(void) {
    uint8_t reg0b = 0;
    if (read_reg(BQ_REG_SYS_STATUS, &reg0b) != ESP_OK) {
        return VBUS_STATUS_NONE;
    }
    uint8_t vbus_stat = (reg0b >> 5) & 0x07;
    return (vbus_status_t)vbus_stat;
}

bool power_is_charging(void) {
    charge_status_t status = power_get_charge_status();
    return status == CHARGE_STATUS_PRE_CHARGE || status == CHARGE_STATUS_FAST_CHARGE;
}

charge_status_t power_get_charge_status(void) {
    uint8_t reg0b = 0;
    if (read_reg(BQ_REG_SYS_STATUS, &reg0b) != ESP_OK) {
        return CHARGE_STATUS_NOT_CHARGING;
    }
    uint8_t chrg_stat = (reg0b >> 3) & 0x03;
    return (charge_status_t)chrg_stat;
}

bool power_enter_shipping_mode(void) {
    // Set BATFET_DIS (REG09[5]=1) to disconnect battery
    // System will only run from USB after this.
    // User must press PW ON / RESET to wake.
    uint8_t current = 0;
    if (read_reg(BQ_REG_MISC, &current) != ESP_OK) {
        LOG_E("POWER", "Failed to read REG09");
        return false;
    }

    if (!write_reg(BQ_REG_MISC, current | (1 << 5))) {
        LOG_E("POWER", "Failed to set BATFET_DIS");
        return false;
    }

    LOG_I("POWER", "Entered shipping mode");
    return true;
}

void power_prepare_gpio_for_sleep(void) {
    LOG_D("POWER", "Preparing GPIO for sleep...");

    // Disable GPIO interrupt at hardware level before sleep
    // The level-triggered wakeup would otherwise cause continuous ISR calls
    gpio_intr_disable(EXP_IRQ_PIN);

    // Set ISR processing flag to prevent issues if ISR somehow fires
    pin_expander_disable_isr();
}

void power_stabilize_gpio_after_sleep(void) {
    LOG_D("POWER", "Stabilizing GPIO after sleep...");

    // 1. Disable GPIO interrupt first (may already be enabled from pin_expander_init after deep sleep reset)
    gpio_intr_disable(EXP_IRQ_PIN);

    // 2. Disable ISR processing flag
    pin_expander_disable_isr();

    // 3. Wait for any key to be released (level-triggered wakeup keeps pin LOW)
    while (pin_expander_any_key_down()) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    vTaskDelay(pdMS_TO_TICKS(30));  // Debounce

    // 4. Clear any buffered keys from wakeup
    while (pin_expander_get_key() != 'x') {}

    // 5. Re-enable ISR processing flag
    pin_expander_enable_isr();

    // 6. Restore edge-triggered interrupt type (wakeup uses level-triggered)
    gpio_set_intr_type(EXP_IRQ_PIN, GPIO_INTR_NEGEDGE);

    // 7. Re-enable GPIO interrupt
    gpio_intr_enable(EXP_IRQ_PIN);

    LOG_D("POWER", "GPIO stabilized");
}

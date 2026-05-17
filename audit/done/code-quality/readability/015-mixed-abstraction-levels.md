---
title: "[015] [LOW] Mixed abstraction levels in main() function"
severity: LOW
domain: Code Readability
lens: code-quality/readability
labels:
  - "audit:code-quality/readability"
---

## Summary
The `app_main()` function in `main.cpp` mixes high-level orchestration with low-level hardware initialization details, making the flow harder to follow.

## Impact
- Function is 258 lines with 20+ distinct initialization steps
- Hard to see the "big picture" without reading every line
- Low-level details (I2C bus init, GPIO config) clutter high-level flow
- Difficult to modify one subsystem without accidentally affecting another

## Evidence
**File: `main/main.cpp:54-258`**
```cpp
extern "C" void app_main(void)
{
    // === STAGE 0: Hardware Minimum ===
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // === STAGE 1: Core Services ===
    if (!EventBus::instance().init()) {
        return;
    }

    if (!usb_cdc_init()) {
        return;
    }

    log_init();

    LOG_I(TAG, "CDC Badge OS v0.5");

    // === TIME VALIDATION ===
    cdc::hal::IRtc* rtc = cdc::hal::getRtcInstance();
    if (rtc) {
        rtc->init();
    }
    if (!rtc || !rtc->isTimeSet()) {
        LOG_W(TAG, "System time not set");
    }

    // === I2C BUS INITIALIZATION ===
    esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();
    if (wakeup_cause == ESP_SLEEP_WAKEUP_EXT1) {
        LOG_D(TAG, "Woke from deep sleep, deinit RTC GPIO");
        rtc_gpio_deinit(EXP_IRQ_PIN);
    }

    LOG_I(TAG, "Initializing I2C bus...");
    s_i2cBus = cdc::hal::getI2cBus0();
    if (s_i2cBus && s_i2cBus->init()) {
        LOG_I(TAG, "I2C bus ready");
    } else {
        LOG_E(TAG, "I2C bus init failed!");
    }

    // === POWER MANAGEMENT INITIALIZATION ===
    LOG_I(TAG, "Initializing Power Management...");
    s_powerManager = cdc::hal::getPowerManagerInstance();
    if (s_powerManager && s_powerManager->init() && s_powerManager->start()) {
        LOG_I(TAG, "Power Management ready (BQ25895)");
        LOG_I(TAG, "Battery: %d%% (%dmV)", s_powerManager->getBatteryPercent(),
              s_powerManager->getBatteryVoltage());
    } else {
        LOG_E(TAG, "Power Management init failed!");
    }

    // ... similar patterns for SLEEP, WIFI, BLUETOOTH, KEYPAD, SECURE ELEMENT ...

    // === ATTESTATION KEY SERVICE ===
    s_attestationService.setSecureElement(s_secureElement);
    s_attestationService.init();
    s_attestationService.start();
    ServiceRegistry::instance().registerService("attestation_key", &s_attestationService);

    // === TROPIC STORAGE CACHE ===
    auto& tropicStorage = cdc::core::TropicStorage::instance();
    tropicStorage.setSecureElement(s_secureElement);
    tropicStorage.init();
    tropicStorage.start();
    ServiceRegistry::instance().registerService("tropic_storage", &tropicStorage);

    // === DISPLAY INITIALIZATION ===
    cdc::hal::IDisplay* display = cdc::hal::getDisplayInstance();
    if (display && display->init() && display->start()) {
        display->showSplash("Waking up...");
    }

    // === UI INITIALIZATION ===
    cdc::ui::UiDeps deps;
    deps.display = display;
    deps.keypad = s_keypad;
    deps.power = s_powerManager;
    deps.sleep = s_sleepController;
    deps.secureElement = s_secureElement;
    cdc::ui::ui_init(deps);

    // === MODULE INITIALIZATION ===
    modules_register_all();
    cdc::core::ModuleRegistry::instance().runAllInitializers();
    cdc::ui::ui_on_modules_ready();

    // === Main Loop ===
    while (true) {
        EventBus::instance().process();
        cdc::serial::SerialCmd::process();
        if (s_powerManager) {
            s_powerManager->update();
        }
        uint32_t nowMs = esp_timer_get_time() / 1000;
        cdc::ui::ui_process(nowMs);
        s_attestationService.onTick(nowMs);
        cdc::core::ModuleRegistry::instance().dispatchTick(nowMs);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

## Recommended Fix
Extract initialization into well-named helper functions:

```cpp
static bool initStorage() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret == ESP_OK;
}

static bool initCoreServices() {
    if (!EventBus::instance().init()) return false;
    if (!usb_cdc_init()) return false;
    log_init();
    return true;
}

static void handleDeepSleepWakeup() {
    esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();
    if (wakeup_cause == ESP_SLEEP_WAKEUP_EXT1) {
        LOG_D(TAG, "Woke from deep sleep, deinit RTC GPIO");
        rtc_gpio_deinit(EXP_IRQ_PIN);
    }
}

static bool initHardware() {
    handleDeepSleepWakeup();
    
    auto* i2c = cdc::hal::getI2cBus0();
    if (!i2c || !i2c->init()) return false;
    
    auto* power = cdc::hal::getPowerManagerInstance();
    if (!power || !power->init() || !power->start()) return false;
    
    // ... other hardware ...
    return true;
}

static void initServices() {
    auto* secureElement = cdc::hal::getSecureElementInstance();
    
    // Attestation key
    static AttestationKeyService attestationService;
    attestationService.setSecureElement(secureElement);
    attestationService.init();
    attestationService.start();
    ServiceRegistry::instance().registerService("attestation_key", &attestationService);
    
    // Tropic storage
    auto& tropicStorage = cdc::core::TropicStorage::instance();
    tropicStorage.setSecureElement(secureElement);
    tropicStorage.init();
    tropicStorage.start();
    ServiceRegistry::instance().registerService("tropic_storage", &tropicStorage);
}

static void initUI() {
    auto* display = cdc::hal::getDisplayInstance();
    if (!display || !display->init() || !display->start()) return;
    
    cdc::ui::UiDeps deps = {
        .display = display,
        .keypad = cdc::hal::getKeypadInstance(),
        .power = cdc::hal::getPowerManagerInstance(),
        .sleep = cdc::hal::getSleepControllerInstance(),
        .secureElement = cdc::hal::getSecureElementInstance(),
    };
    cdc::ui::ui_init(deps);
}

extern "C" void app_main(void) {
    // STAGE 0: Hardware Minimum
    if (!initStorage()) return;

    // STAGE 1: Core Services
    if (!initCoreServices()) return;
    
    LOG_I(TAG, "CDC Badge OS v0.5");

    // STAGE 2: Hardware
    if (!initHardware()) return;

    // STAGE 3: Services
    initServices();

    // STAGE 4: UI
    initUI();

    // STAGE 5: Modules
    modules_register_all();
    cdc::core::ModuleRegistry::instance().runAllInitializers();
    cdc::ui::ui_on_modules_ready();

    // Main loop
    while (true) {
        EventBus::instance().process();
        cdc::serial::SerialCmd::process();
        cdc::hal::getPowerManagerInstance()->update();
        cdc::ui::ui_process(esp_timer_get_time() / 1000);
        cdc::core::AttestationKeyService::onTick(esp_timer_get_time() / 1000);
        cdc::core::ModuleRegistry::instance().dispatchTick(esp_timer_get_time() / 1000);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

## References
- Clean Code: "Functions should be small and do one thing" - Robert C. Martin
- C++ Core Guidelines: F.1 - Keep functions small

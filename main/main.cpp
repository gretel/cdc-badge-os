/**
 * CDC Badge OS v0.5 - Modular Rewrite
 * USB CDC + Serial + Display + UI
 */

#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "cdc_log.h"

#include "cdc_core/ServiceRegistry.h"
#include "cdc_core/EventBus.h"
#include "cdc_core/ModuleRegistry.h"
#include "cdc_core/SystemLock.h"
#include "cdc_core/TropicStorage.h"
#include "cdc_core/AttestationKeyService.h"
#include "modules_init.gen.h"  // Auto-generated module registrations
#include "usb_badge/usb_cdc.h"
#include "serial_cmd/SerialCmd.h"
#include "cdc_hal/IDisplay.h"
#include "cdc_hal/II2cBus.h"
#include "cdc_hal/IKeypad.h"
#include "cdc_hal/IPowerManager.h"
#include "cdc_hal/ISecureElement.h"
#include "cdc_hal/ISleepController.h"
#include "cdc_hal/IWifiController.h"
#include "cdc_hal/IBluetoothController.h"
#include "cdc_hal/hw_config.h"
#include "cdc_hal/IRtc.h"
#include "cdc_os_ui/AppUi.h"
#include "driver/rtc_io.h"
#include "esp_sleep.h"
#include "esp_timer.h"

static const char* TAG = "BOOT";

using namespace cdc::core;

/** \brief Cached HAL/service singleton pointers used during boot sequence. */
static cdc::hal::II2cBus* s_i2cBus = nullptr;
static cdc::hal::IKeypad* s_keypad = nullptr;
static cdc::hal::IPowerManager* s_powerManager = nullptr;
static cdc::hal::ISecureElement* s_secureElement = nullptr;
static cdc::hal::ISleepController* s_sleepController = nullptr;
static cdc::hal::IDisplay* s_display = nullptr;
static cdc::core::AttestationKeyService s_attestationService;
static esp_sleep_wakeup_cause_t s_wakeupCause = ESP_SLEEP_WAKEUP_UNDEFINED;

/**
 * \brief Draws a modal-style system-lockdown halt screen before deep sleep.
 * \param reason Reason captured by \ref SystemLock.
 * \param detail Optional detail string captured by \ref SystemLock.
 */
static void lockdownShutdownHandler(cdc::core::LockdownReason reason,
                                    const char* detail) {
    const char* reasonText;
    switch (reason) {
        case cdc::core::LockdownReason::TR01_ALARM_MODE:
            reasonText = "Secure element alarm"; break;
        case cdc::core::LockdownReason::TR01_UNREACHABLE:
            reasonText = "Secure element offline"; break;
        case cdc::core::LockdownReason::TR01_INIT_FAILED:
            reasonText = "Secure element init failed"; break;
        default:
            reasonText = "Unknown failure"; break;
    }

    LOG_E(TAG, "SYSTEM LOCKDOWN: %s%s%s", reasonText,
          detail ? " - " : "", detail ? detail : "");

    if (!s_display) {
        return;
    }

    const int16_t w = static_cast<int16_t>(s_display->getWidth());
    const int16_t h = static_cast<int16_t>(s_display->getHeight());
    constexpr int16_t kMargin = 8;
    const int16_t modalX = kMargin;
    const int16_t modalY = kMargin;
    const int16_t modalW = static_cast<int16_t>(w - 2 * kMargin);
    const int16_t modalH = static_cast<int16_t>(h - 2 * kMargin);

    s_display->clear();
    s_display->fillRect(modalX, modalY, modalW, modalH, 0xFFFF);
    s_display->drawRect(modalX, modalY, modalW, modalH, 0x0000);
    s_display->drawRect(modalX + 1, modalY + 1,
                        static_cast<int16_t>(modalW - 2),
                        static_cast<int16_t>(modalH - 2), 0x0000);

    constexpr int16_t kHeaderHeight = 18;
    s_display->fillRect(static_cast<int16_t>(modalX + 2),
                        static_cast<int16_t>(modalY + 2),
                        static_cast<int16_t>(modalW - 4),
                        kHeaderHeight, 0x0000);
    s_display->setTextColor(0xFFFF);
    s_display->setTextSize(2);
    s_display->setCursor(static_cast<int16_t>(modalX + 12),
                         static_cast<int16_t>(modalY + 4));
    s_display->print("SYSTEM LOCKED");

    s_display->setTextColor(0x0000);
    s_display->setTextSize(1);

    int16_t cursorY = static_cast<int16_t>(modalY + kHeaderHeight + 12);
    s_display->setCursor(static_cast<int16_t>(modalX + 10), cursorY);
    s_display->print(reasonText);

    if (detail && *detail) {
        cursorY = static_cast<int16_t>(cursorY + 14);
        s_display->setCursor(static_cast<int16_t>(modalX + 10), cursorY);
        s_display->printf("Detail: %s", detail);
    }

    s_display->setCursor(static_cast<int16_t>(modalX + 10),
                         static_cast<int16_t>(modalY + modalH - 14));
    s_display->print("Power cycle to recover");

    s_display->flushSync(cdc::hal::RefreshMode::FULL);
}

/**
 * \brief Stage 0: initializes NVS flash storage with automatic erase on
 *        version-mismatch or out-of-pages errors.
 * \return `true` on success.
 */
static bool initNvs() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    return ret == ESP_OK;
}

/**
 * \brief Stage 1: brings up event bus, USB CDC and the logging subsystem.
 * \return `true` if all core services initialized successfully.
 */
static bool initCoreServices() {
    if (!EventBus::instance().init()) {
        return false;  // Cannot log yet, USB not ready
    }

    if (!usb_cdc_init()) {
        return false;  // USB failed, no output channel available
    }

    log_init();

    LOG_I(TAG, "=================================");
    LOG_I(TAG, "CDC Badge OS v0.5");
    LOG_I(TAG, "Modular Rewrite");
    LOG_I(TAG, "=================================");

    LOG_I(TAG, "EventBus ready");
    LOG_I(TAG, "USB CDC ready");
    LOG_I(TAG, "ServiceRegistry ready (capacity: %u)", ServiceRegistry::MAX_SERVICES);
    return true;
}

/**
 * \brief Initializes the RTC and warns when the system time has not been set.
 */
static void initRtc() {
    cdc::hal::IRtc* rtc = cdc::hal::getRtcInstance();
    if (rtc) {
        rtc->init();
    }
    if (!rtc || !rtc->isTimeSet()) {
        LOG_W(TAG, "System time not set");
    }
}

/**
 * \brief Caches the wakeup cause and releases RTC GPIO when resuming from deep
 *        sleep so that subsequent I2C bus init can re-claim the IRQ pin.
 */
static void handleWakeupAndReleaseRtcGpio() {
    s_wakeupCause = esp_sleep_get_wakeup_cause();
    if (s_wakeupCause == ESP_SLEEP_WAKEUP_EXT1) {
        LOG_D(TAG, "Woke from deep sleep, deinit RTC GPIO");
        rtc_gpio_deinit(EXP_IRQ_PIN);
    }
}

/**
 * \brief Brings up the I2C bus shared by the on-board peripherals.
 */
static void initI2cBus() {
    LOG_I(TAG, "Initializing I2C bus...");
    s_i2cBus = cdc::hal::getI2cBus0();
    if (s_i2cBus && s_i2cBus->init()) {
        LOG_I(TAG, "I2C bus ready");
    } else {
        LOG_E(TAG, "I2C bus init failed!");
    }
}

/**
 * \brief Initializes the BQ25895 power manager and reports current battery
 *        state to the log.
 */
static void initPowerManager() {
    LOG_I(TAG, "Initializing Power Management...");
    s_powerManager = cdc::hal::getPowerManagerInstance();
    if (s_powerManager && s_powerManager->init() && s_powerManager->start()) {
        LOG_I(TAG, "Power Management ready (BQ25895)");
        LOG_I(TAG, "Battery: %d%% (%dmV)", s_powerManager->getBatteryPercent(),
              s_powerManager->getBatteryVoltage());
    } else {
        LOG_E(TAG, "Power Management init failed!");
    }
}

/**
 * \brief Initializes the sleep controller which manages light/deep sleep.
 */
static void initSleepController() {
    LOG_I(TAG, "Initializing Sleep Controller...");
    s_sleepController = cdc::hal::getSleepControllerInstance();
    if (s_sleepController && s_sleepController->init() && s_sleepController->start()) {
        LOG_I(TAG, "Sleep Controller ready (interval: %lus)",
              (unsigned long)s_sleepController->getLightSleepInterval());
    } else {
        LOG_E(TAG, "Sleep Controller init failed!");
    }
}

/**
 * \brief Initializes the WiFi controller HAL singleton.
 */
static void initWifiController() {
    LOG_I(TAG, "Initializing WiFi Controller...");
    cdc::hal::IWifiController* wifiController = cdc::hal::getWifiControllerInstance();
    if (wifiController && wifiController->init() && wifiController->start()) {
        LOG_I(TAG, "WiFi Controller ready");
    } else {
        LOG_E(TAG, "WiFi Controller init failed!");
    }
}

/**
 * \brief Initializes the Bluetooth controller HAL singleton.
 */
static void initBluetoothController() {
    LOG_I(TAG, "Initializing Bluetooth Controller...");
    cdc::hal::IBluetoothController* btController = cdc::hal::getBluetoothControllerInstance();
    if (btController && btController->init() && btController->start()) {
        LOG_I(TAG, "Bluetooth Controller ready");
    } else {
        LOG_E(TAG, "Bluetooth Controller init failed!");
    }
}

/**
 * \brief Initializes the keypad input scanner.
 */
static void initKeypad() {
    LOG_I(TAG, "Initializing Keypad...");
    s_keypad = cdc::hal::getKeypadInstance();
    if (s_keypad && s_keypad->init()) {
        LOG_I(TAG, "Keypad ready (12 keys)");
    } else {
        LOG_E(TAG, "Keypad init failed!");
    }
}

/**
 * \brief Initializes the TROPIC01 secure element and starts an active session.
 */
static void initSecureElement() {
    LOG_I(TAG, "Initializing Secure Element...");
    s_secureElement = cdc::hal::getSecureElementInstance();
    if (s_secureElement && s_secureElement->init() && s_secureElement->start()) {
        if (s_secureElement->sessionStart()) {
            LOG_I(TAG, "Secure Element ready (TROPIC01, session active)");
        } else {
            LOG_W(TAG, "Secure Element initialized but session start failed");
        }
    } else {
        LOG_E(TAG, "Secure Element init failed!");
    }
}

/**
 * \brief Brings up all hardware peripherals in dependency order.
 *
 * Combines I2C bus, power management, sleep controller, radios, keypad and
 * the secure element. Wakeup-cause handling is performed first so that the
 * RTC GPIO is released before the bus comes up.
 */
static void initHardware() {
    handleWakeupAndReleaseRtcGpio();
    initI2cBus();
    initPowerManager();
    initSleepController();
    initWifiController();
    initBluetoothController();
    initKeypad();
    initSecureElement();
}

/**
 * \brief Registers the attestation-key service with the global ServiceRegistry.
 */
static void initAttestationService() {
    s_attestationService.setSecureElement(s_secureElement);
    s_attestationService.init();
    s_attestationService.start();
    ServiceRegistry::instance().registerService("attestation_key", &s_attestationService);
}

/**
 * \brief Initializes and registers the TROPIC01 storage cache service.
 */
static void initTropicStorage() {
    auto& tropicStorage = cdc::core::TropicStorage::instance();
    tropicStorage.setSecureElement(s_secureElement);
    tropicStorage.init();
    tropicStorage.start();
    ServiceRegistry::instance().registerService("tropic_storage", &tropicStorage);
    LOG_I(TAG, "TropicStorage cache ready");
}

/**
 * \brief Initializes the serial command processor over USB CDC.
 */
static void initSerialCommandInterface() {
    cdc::serial::SerialCmd::init();
    LOG_I(TAG, "Serial Command Interface ready");
}

/**
 * \brief Brings up high-level OS services that depend on hardware being ready.
 */
static void initSystemServices() {
    initAttestationService();
    initTropicStorage();
    initSerialCommandInterface();
}

/**
 * \brief Initializes the e-paper display and shows a boot splash.
 *
 * The wakeup cause cached during hardware init determines whether the splash
 * announces a regular boot or a deep-sleep wakeup.
 */
static void initDisplay() {
    LOG_I(TAG, "Initializing Display...");
    s_display = cdc::hal::getDisplayInstance();
    if (s_display && s_display->init() && s_display->start()) {
        LOG_I(TAG, "Display ready (%ux%u)", s_display->getWidth(), s_display->getHeight());

        cdc::core::SystemLock::instance().setShutdownHandler(lockdownShutdownHandler);

        if (s_wakeupCause == ESP_SLEEP_WAKEUP_EXT1) {
            s_display->showSplash("Waking up...");
        } else {
            s_display->showSplash();
        }
        LOG_I(TAG, "Splash screen done");
    } else {
        LOG_E(TAG, "Display init failed!");
    }
}

/**
 * \brief Initializes the App UI layer with the previously prepared HAL deps.
 */
static void initUi() {
    LOG_I(TAG, "Initializing UI...");
    cdc::ui::UiDeps deps;
    deps.display = s_display;
    deps.keypad = s_keypad;
    deps.power = s_powerManager;
    deps.sleep = s_sleepController;
    deps.secureElement = s_secureElement;
    cdc::ui::ui_init(deps);
}

/**
 * \brief Registers all auto-generated modules and runs their initializers.
 *
 * After modules are ready the UI menus are rebuilt to surface module-provided
 * entries.
 */
static void initModules() {
    modules_register_all();

    LOG_I(TAG, "Initializing modules...");
    cdc::core::ModuleRegistry::instance().runAllInitializers();

    cdc::ui::ui_on_modules_ready();
}

/**
 * \brief Final startup step: completes USB CDC enumeration and prints banner.
 */
static void startApp() {
    usb_cdc_start();

    LOG_I(TAG, "=================================");
    LOG_I(TAG, "System ready. Entering main loop.");
    LOG_I(TAG, "=================================");
}

/**
 * \brief Single iteration of the cooperative main loop.
 *
 * Drains the event bus, services the serial console, ticks power management,
 * advances the UI and dispatches a tick to all modules.
 */
static void runMainLoopIteration() {
    // Hard lockdown gate: if any subsystem latched the lockdown flag, run the
    // halt-screen handler and deep-sleep without enabling wake sources.
    SystemLock::instance().enforceIfLocked();

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

/**
 * \brief Main firmware entry point.
 */
extern "C" void app_main(void)
{
    // STAGE 0: Hardware Minimum
    initNvs();

    // STAGE 1: Core Services
    if (!initCoreServices()) return;

    // STAGE 2: Time
    initRtc();

    // STAGE 3: Hardware Peripherals
    initHardware();

    // STAGE 4: System Services
    initSystemServices();

    // STAGE 5: Display & UI
    initDisplay();
    initUi();

    // STAGE 6: Modules
    initModules();

    // STAGE 7: Final startup
    startApp();

    // Main loop
    while (true) {
        runMainLoopIteration();
    }
}

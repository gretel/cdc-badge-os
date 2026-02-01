/**
 * BLE Serial Module Implementation
 *
 * Serial commands over Bluetooth Low Energy using Nordic UART Service.
 */

#include "mod_ble_serial/BleSerialModule.h"
#include "mod_ble_serial/BleUartService.h"
#include "cdc_core/ModuleRegistry.h"
#include "cdc_hal/IBluetoothController.h"
#include "cdc_ui/I18n.h"
#include "cdc_ui/ViewStack.h"
#include "cdc_views/ToastView.h"
#include "cdc_log.h"
#include "nvs.h"
#include <cstring>

static const char* TAG = "BLE_SERIAL";

namespace cdc::mod_ble_serial {

// =============================================================================
// I18n Strings
// =============================================================================

uint16_t BleSerialModule::s_strIdBase = 0;

static constexpr uint16_t STR_BLE_SERIAL = 0;
static constexpr uint16_t STR_ENABLED = 1;
static constexpr uint16_t STR_DISABLED = 2;
static constexpr uint16_t STR_COUNT = 3;

void BleSerialModule::registerStrings() {
    auto& i18n = ui::I18n::instance();
    s_strIdBase = i18n.registerModule("mod_ble_serial", STR_COUNT);
    if (s_strIdBase == 0) {
        LOG_E(TAG, "Failed to register i18n strings");
        return;
    }

    // English
    i18n.registerTranslation(s_strIdBase + STR_BLE_SERIAL, ui::Language::EN, "BLE Serial");
    i18n.registerTranslation(s_strIdBase + STR_ENABLED, ui::Language::EN, "Enabled");
    i18n.registerTranslation(s_strIdBase + STR_DISABLED, ui::Language::EN, "Disabled");

    // German
    i18n.registerTranslation(s_strIdBase + STR_BLE_SERIAL, ui::Language::DE, "BLE Seriell");
    i18n.registerTranslation(s_strIdBase + STR_ENABLED, ui::Language::DE, "Aktiviert");
    i18n.registerTranslation(s_strIdBase + STR_DISABLED, ui::Language::DE, "Deaktiviert");

    LOG_I(TAG, "Registered i18n strings (base=%d)", s_strIdBase);
}

const char* BleSerialModule::mstr(uint16_t offset) const {
    return ui::tr(s_strIdBase + offset);
}

// =============================================================================
// Console Hooks
// =============================================================================

// Static callbacks for console hooks
static void bleOutputHook(const char* data, size_t len) {
    auto& uart = BleUartService::instance();
    if (uart.isConnected() && uart.isInitialized()) {
        uart.send(reinterpret_cast<const uint8_t*>(data), len);
    }
}

static bool bleInputAvailableHook() {
    auto& uart = BleUartService::instance();
    return uart.isConnected() && uart.available() > 0;
}

static int bleInputGetcharHook() {
    auto& uart = BleUartService::instance();
    return uart.getchar();
}

void BleSerialModule::registerConsoleHooks() {
    console_register_output_hook(bleOutputHook);
    console_register_input_hook(bleInputAvailableHook, bleInputGetcharHook);
    LOG_I(TAG, "Console hooks registered");
}

void BleSerialModule::unregisterConsoleHooks() {
    console_register_output_hook(nullptr);
    console_register_input_hook(nullptr, nullptr);
    LOG_I(TAG, "Console hooks unregistered");
}

// =============================================================================
// Settings
// =============================================================================

void BleSerialModule::loadSettings() {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) == ESP_OK) {
        uint8_t val = 0;
        if (nvs_get_u8(nvs, "enabled", &val) == ESP_OK) {
            enabled_ = (val != 0);
        }
        nvs_close(nvs);
    }
}

void BleSerialModule::saveSettings() {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_u8(nvs, "enabled", enabled_ ? 1 : 0);
        nvs_commit(nvs);
        nvs_close(nvs);
    }
}

// =============================================================================
// Module Implementation
// =============================================================================

BleSerialModule& BleSerialModule::instance() {
    static BleSerialModule inst;
    return inst;
}

bool BleSerialModule::init() {
    LOG_I(TAG, "Initializing BLE Serial module");

    registerStrings();
    loadSettings();

    state_ = core::ServiceState::INITIALIZED;
    return true;
}

bool BleSerialModule::start() {
    if (state_ != core::ServiceState::INITIALIZED &&
        state_ != core::ServiceState::STOPPED) {
        return false;
    }

    // If auto-enable is set and BLE is enabled, start the service
    if (enabled_) {
        auto* ble = hal::getBluetoothControllerInstance();
        if (ble && ble->isEnabled()) {
            auto& uart = BleUartService::instance();
            if (uart.init()) {
                registerConsoleHooks();
                LOG_I(TAG, "BLE Serial service started");
            }
        }
    }

    state_ = core::ServiceState::STARTED;
    return true;
}

void BleSerialModule::stop() {
    if (enabled_) {
        unregisterConsoleHooks();
        BleUartService::instance().deinit();
    }
    state_ = core::ServiceState::STOPPED;
}

void BleSerialModule::toggle() {
    auto* ble = hal::getBluetoothControllerInstance();
    if (!ble) {
        ui::showToastError("BLE n/a");
        return;
    }

    if (!ble->isEnabled()) {
        ui::showToastError("BLE disabled");
        return;
    }

    enabled_ = !enabled_;
    saveSettings();

    if (enabled_) {
        auto& uart = BleUartService::instance();
        if (uart.init()) {
            registerConsoleHooks();
            ui::showToastSuccess(mstr(STR_ENABLED));
        } else {
            enabled_ = false;
            saveSettings();
            ui::showToastError("Init failed");
        }
    } else {
        unregisterConsoleHooks();
        BleUartService::instance().deinit();
        ui::showToastInfo(mstr(STR_DISABLED));
    }
}

uint8_t BleSerialModule::getMenuItems(core::ModuleMenuItem* items, uint8_t maxItems) {
    if (!items || maxItems == 0) return 0;

    // Build label with status
    snprintf(labelBuf_, LABEL_BUF_SIZE, "%s: %s",
             mstr(STR_BLE_SERIAL),
             enabled_ ? mstr(STR_ENABLED) : mstr(STR_DISABLED));

    items[0].label = labelBuf_;
    items[0].priority = 50;
    items[0].getView = nullptr;  // No view - toggle action handled by menu
    items[0].isVisible = nullptr;
    items[0].moduleName = getName();
    items[0].location = core::MenuLocation::BLUETOOTH_MENU;

    return 1;
}

void BleSerialModule::onTick(uint32_t nowMs) {
    (void)nowMs;
    // Could check connection state changes here if needed
}

} // namespace cdc::mod_ble_serial

// =============================================================================
// Registration
// =============================================================================

extern "C" void mod_ble_serial_register() {
    cdc::core::ModuleRegistry::instance().registerInitializer([]() {
        auto& moduleReg = cdc::core::ModuleRegistry::instance();
        auto& module = cdc::mod_ble_serial::BleSerialModule::instance();

        moduleReg.registerModule(&module);

        if (!module.init()) {
            moduleReg.reportModuleError(module.getName(), "Init failed");
            return;
        }
        module.start();
    });
}

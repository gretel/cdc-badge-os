/**
 * \file
 * \brief BLE serial module wiring NUS transport to console hooks and UI toggle.
 */

#include "mod_ble_serial/BleSerialModule.h"
#include "mod_ble_serial/BleUartService.h"
#include "cdc_core/ModuleRegistry.h"
#include "cdc_hal/IBluetoothController.h"
#include "cdc_ui/I18n.h"
#include "cdc_ui/ViewStack.h"
#include "cdc_views/ToastView.h"
#include "cdc_views/ConfirmView.h"
#include "cdc_log.h"
#include "nvs.h"
#include <cstring>

static const char* TAG = "BLE_SERIAL";

namespace cdc::mod_ble_serial {

/** \brief Module-local i18n string offsets. */

uint16_t BleSerialModule::s_strIdBase = 0;

static constexpr uint16_t STR_BLE_SERIAL = 0;
static constexpr uint16_t STR_ENABLED = 1;
static constexpr uint16_t STR_DISABLED = 2;
static constexpr uint16_t STR_COUNT = 3;

/**
 * \brief Registers BLE-serial module translations.
 */
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

/**
 * \brief Resolves module-localized string by offset.
 * \param offset Module string-table offset.
 * \return Translated string pointer.
 */
const char* BleSerialModule::mstr(uint16_t offset) const {
    return ui::tr(s_strIdBase + offset);
}

/** \brief Console hook bridge between shell I/O and BLE UART transport. */

/**
 * \brief Console output hook that forwards bytes to the BLE UART service.
 * \param data Pointer to output bytes.
 * \param len Number of bytes to forward.
 * \return void
 */
static void bleOutputHook(const char* data, size_t len) {
    auto& uart = BleUartService::instance();
    if (uart.isConnected() && uart.isInitialized()) {
        uart.send(reinterpret_cast<const uint8_t*>(data), len);
    }
}

/**
 * \brief Console input-available hook using BLE UART RX queue.
 * \return `true` when BLE serial has pending input.
 */
static bool bleInputAvailableHook() {
    auto& uart = BleUartService::instance();
    return uart.isConnected() && uart.available() > 0;
}

/**
 * \brief Console getchar hook reading one byte from BLE UART.
 * \return Character value or negative when unavailable.
 */
static int bleInputGetcharHook() {
    auto& uart = BleUartService::instance();
    return uart.getchar();
}

/**
 * \brief Registers BLE-backed console input/output hooks.
 */
void BleSerialModule::registerConsoleHooks() {
    console_register_output_hook(bleOutputHook);
    console_register_input_hook(bleInputAvailableHook, bleInputGetcharHook);
    LOG_I(TAG, "Console hooks registered");
}

/**
 * \brief Unregisters BLE-backed console hooks.
 */
void BleSerialModule::unregisterConsoleHooks() {
    console_register_output_hook(nullptr);
    console_register_input_hook(nullptr, nullptr);
    LOG_I(TAG, "Console hooks unregistered");
}

/** \brief Pairing-confirmation UI callbacks. */

/**
 * \brief Registers numeric-comparison pairing prompt callback.
 */
void BleSerialModule::registerPairingCallback() {
    auto* ble = hal::getBluetoothControllerInstance();
    if (!ble) return;

    ble->setNumericComparisonCallback([](uint16_t connHandle, uint32_t passkey) {
        auto& self = BleSerialModule::instance();
        self.pairingConnHandle_ = connHandle;

        char msg[48];
        snprintf(msg, sizeof(msg), "BLE Pairing?\n%06lu", (unsigned long)passkey);

        ui::showConfirm(msg, onPairingConfirm, onPairingReject,
                        ui::ConfirmView::Icon::QUESTION, &self);
    });
}

/**
 * \brief Confirms pending BLE pairing request.
 * \param userData Module instance pointer.
 */
void BleSerialModule::onPairingConfirm(void* userData) {
    auto* self = static_cast<BleSerialModule*>(userData);
    auto* ble = hal::getBluetoothControllerInstance();
    if (ble && self) {
        ble->respondToNumericComparison(self->pairingConnHandle_, true);
    }
}

/**
 * \brief Rejects pending BLE pairing request.
 * \param userData Module instance pointer.
 */
void BleSerialModule::onPairingReject(void* userData) {
    auto* self = static_cast<BleSerialModule*>(userData);
    auto* ble = hal::getBluetoothControllerInstance();
    if (ble && self) {
        ble->respondToNumericComparison(self->pairingConnHandle_, false);
    }
}

/** \brief Persistent settings helpers. */

/**
 * \brief Loads module enable-state from NVS.
 */
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

/**
 * \brief Persists module enable-state to NVS.
 */
void BleSerialModule::saveSettings() {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_u8(nvs, "enabled", enabled_ ? 1 : 0);
        nvs_commit(nvs);
        nvs_close(nvs);
    }
}

/** \brief BLE serial module lifecycle implementation. */

/**
 * \brief Returns singleton BLE serial module instance.
 * \return Module singleton reference.
 */
BleSerialModule& BleSerialModule::instance() {
    static BleSerialModule inst;
    return inst;
}

/**
 * \brief Initializes BLE serial module resources and settings.
 * \return `true` on successful initialization.
 */
bool BleSerialModule::init() {
    LOG_I(TAG, "Initializing BLE Serial module");

    registerStrings();
    loadSettings();

    state_ = core::ServiceState::INITIALIZED;
    return true;
}

/**
 * \brief Starts BLE serial module and optionally auto-enables service.
 * \return `true` if start transition succeeded.
 */
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
                registerPairingCallback();
                LOG_I(TAG, "BLE Serial service started");
            }
        }
    }

    state_ = core::ServiceState::STARTED;
    return true;
}

/**
 * \brief Stops BLE serial module and deinitializes UART service when enabled.
 */
void BleSerialModule::stop() {
    if (enabled_) {
        unregisterConsoleHooks();
        BleUartService::instance().deinit();
    }
    state_ = core::ServiceState::STOPPED;
}

/**
 * \brief Toggles BLE serial service state and updates persisted setting.
 */
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
            registerPairingCallback();
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

/**
 * \brief Provides Bluetooth-menu item for BLE serial toggle.
 * \param items Output menu item array.
 * \param maxItems Maximum writable entries.
 * \return Number of populated menu items.
 */
uint8_t BleSerialModule::getMenuItems(core::ModuleMenuItem* items, uint8_t maxItems) {
    if (!items || maxItems == 0) return 0;

    // Build label with status
    snprintf(labelBuf_, LABEL_BUF_SIZE, "%s: %s",
             mstr(STR_BLE_SERIAL),
             enabled_ ? mstr(STR_ENABLED) : mstr(STR_DISABLED));

    items[0].label = labelBuf_;
    items[0].priority = 50;
    items[0].getView = nullptr;
    items[0].isVisible = nullptr;
    items[0].moduleName = getName();
    items[0].location = core::MenuLocation::BLUETOOTH_MENU;
    items[0].onSelect = []() { BleSerialModule::instance().toggle(); };

    return 1;
}

/**
 * \brief Periodic module tick hook.
 * \param nowMs Current uptime in milliseconds.
 */
void BleSerialModule::onTick(uint32_t nowMs) {
    (void)nowMs;
    // Could check connection state changes here if needed
}

} // namespace cdc::mod_ble_serial

/**
 * \brief Registers BLE serial module initializer.
 */
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

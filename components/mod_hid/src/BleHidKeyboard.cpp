/**
 * \file
 * \brief BLE HID Keyboard implementation via IBluetoothController API.
 *
 * Uses IBluetoothController API exclusively - no direct NimBLE dependency.
 */

#include "mod_hid/BleHidKeyboard.h"
#include "mod_hid/KeyboardLayout.h"
#include "cdc_hal/IBluetoothController.h"
#include "cdc_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstring>
#include <cstdio>

static const char* TAG = "BleHID";

/** \brief NVS namespace and key names for persisted HID settings. */
static constexpr const char* NVS_NAMESPACE = "mod_hid";
static constexpr const char* NVS_KEY_UNICODE = "unicode";

namespace cdc::mod_hid {

/** \brief HID report descriptor exported by KeyboardLayout.cpp. */
extern const uint8_t* getHidReportMap();
extern size_t getHidReportMapSize();

/** \brief Standard 8-byte boot keyboard input report payload. */
struct KeyboardReport {
    uint8_t modifier;
    uint8_t reserved;
    uint8_t keycodes[6];
};

/** \brief Value of the HID Information characteristic (v1.11, generic keyboard). */
static const uint8_t HID_INFO[] = {
    0x11, 0x01,  // HID version 1.11
    0x00,        // Country code (0 = not localized)
    0x02         // Flags: Normally connectable, not remote wake
};

/** \brief HID protocol mode (`0` boot, `1` report). */
static uint8_t s_protocolMode = 1;

/** \brief Last transmitted keyboard report state. */
static KeyboardReport s_currentReport = {};

/** \brief Handle assigned by stack for the keyboard input report characteristic. */
static uint16_t s_reportHandle = 0;

/** \brief Persistent GATT characteristic storage (must outlive registration). */
static hal::GattCharacteristic s_gattChars[5];

/** \brief Persistent GATT service definition. */
static hal::GattServiceDef s_gattSvcDef;

/** \brief Singleton pointer used by static callbacks. */
static BleHidKeyboard* s_instance = nullptr;

// ============================================================================
// Singleton
// ============================================================================

BleHidKeyboard& BleHidKeyboard::instance() {
    static BleHidKeyboard inst;
    return inst;
}

// ============================================================================
// Lifecycle
// ============================================================================

bool BleHidKeyboard::init() {
    if (initialized_) return true;

    s_instance = this;
    loadSettings();

    auto* ble = hal::getBluetoothControllerInstance();
    if (!ble) {
        LOG_E(TAG, "Bluetooth controller not available");
        return false;
    }

    if (!ble->isEnabled()) {
        if (!ble->enable()) {
            LOG_E(TAG, "Failed to enable Bluetooth");
            return false;
        }
    }

    // Wait for BLE stack to sync
    for (int i = 0; i < 50 && !ble->isEnabled(); i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    using namespace hal;

    // HID Information (read-only)
    s_gattChars[0].uuid = BleUuid::from16(0x2A4A);
    s_gattChars[0].properties = GattProp::READ;
    s_gattChars[0].permissions = GattPerm::READ;
    s_gattChars[0].valueHandle = nullptr;
    s_gattChars[0].onWrite = nullptr;
    s_gattChars[0].onRead = [](uint16_t, uint16_t, uint8_t* buf, uint16_t* len) -> int {
        uint16_t copyLen = sizeof(HID_INFO);
        if (copyLen > *len) copyLen = *len;
        memcpy(buf, HID_INFO, copyLen);
        *len = copyLen;
        return 0;
    };

    // Report Map (read-only)
    s_gattChars[1].uuid = BleUuid::from16(0x2A4B);
    s_gattChars[1].properties = GattProp::READ;
    s_gattChars[1].permissions = GattPerm::READ;
    s_gattChars[1].valueHandle = nullptr;
    s_gattChars[1].onWrite = nullptr;
    s_gattChars[1].onRead = [](uint16_t, uint16_t, uint8_t* buf, uint16_t* len) -> int {
        size_t mapSize = getHidReportMapSize();
        uint16_t copyLen = static_cast<uint16_t>(mapSize > *len ? *len : mapSize);
        memcpy(buf, getHidReportMap(), copyLen);
        *len = copyLen;
        return 0;
    };

    // Keyboard Input Report (read + notify)
    s_gattChars[2].uuid = BleUuid::from16(0x2A4D);
    s_gattChars[2].properties = GattProp::READ | GattProp::NOTIFY;
    s_gattChars[2].permissions = GattPerm::READ;
    s_gattChars[2].valueHandle = &s_reportHandle;
    s_gattChars[2].onWrite = nullptr;
    s_gattChars[2].onRead = [](uint16_t, uint16_t, uint8_t* buf, uint16_t* len) -> int {
        uint16_t copyLen = sizeof(s_currentReport);
        if (copyLen > *len) copyLen = *len;
        memcpy(buf, &s_currentReport, copyLen);
        *len = copyLen;
        return 0;
    };

    // HID Control Point (write-only, no response)
    s_gattChars[3].uuid = BleUuid::from16(0x2A4C);
    s_gattChars[3].properties = GattProp::WRITE_NO_RSP;
    s_gattChars[3].permissions = GattPerm::WRITE;
    s_gattChars[3].valueHandle = nullptr;
    s_gattChars[3].onRead = nullptr;
    s_gattChars[3].onWrite = [](uint16_t, uint16_t, const uint8_t* data, uint16_t len) -> int {
        if (len >= 1 && s_instance) {
            if (data[0] == 0) s_instance->onHostSuspend();
            else if (data[0] == 1) s_instance->onHostResume();
        }
        return 0;
    };

    // Protocol Mode (read + write-no-response)
    s_gattChars[4].uuid = BleUuid::from16(0x2A4E);
    s_gattChars[4].properties = GattProp::READ | GattProp::WRITE_NO_RSP;
    s_gattChars[4].permissions = GattPerm::READ | GattPerm::WRITE;
    s_gattChars[4].valueHandle = nullptr;
    s_gattChars[4].onRead = [](uint16_t, uint16_t, uint8_t* buf, uint16_t* len) -> int {
        buf[0] = s_protocolMode;
        *len = 1;
        return 0;
    };
    s_gattChars[4].onWrite = [](uint16_t, uint16_t, const uint8_t* data, uint16_t len) -> int {
        if (len >= 1) s_protocolMode = data[0];
        return 0;
    };

    // Register GATT service via API
    s_gattSvcDef.uuid = BleUuid::from16(0x1812);  // HID Service
    s_gattSvcDef.characteristics = s_gattChars;
    s_gattSvcDef.numCharacteristics = 5;

    if (!ble->registerGattService(s_gattSvcDef)) {
        LOG_E(TAG, "Failed to register HID GATT service");
        return false;
    }

    // Register connection callbacks via API
    ble->addConnectionCallback([](uint16_t connHandle) {
        if (s_instance) s_instance->onConnect(connHandle);
    });
    ble->addDisconnectionCallback([](uint16_t connHandle, int reason) {
        if (s_instance) s_instance->onDisconnect(connHandle, reason);
    });

    initialized_ = true;
    snprintf(statusText_, sizeof(statusText_), "Ready (not advertising)");
    LOG_I(TAG, "BLE HID Keyboard initialized (reportHandle=%d)", s_reportHandle);
    return true;
}

void BleHidKeyboard::deinit() {
    if (!initialized_) return;

    stopAdvertising();
    s_instance = nullptr;
    initialized_ = false;
    snprintf(statusText_, sizeof(statusText_), "Not initialized");
    LOG_I(TAG, "BLE HID Keyboard deinitialized");
}

// ============================================================================
// Connection state
// ============================================================================

bool BleHidKeyboard::isConnected() const {
    auto* ble = hal::getBluetoothControllerInstance();
    return ble && ble->isConnected();
}

// ============================================================================
// Advertising via API
// ============================================================================

bool BleHidKeyboard::startAdvertising() {
    if (!initialized_ || advertising_) return advertising_;

    auto* ble = hal::getBluetoothControllerInstance();
    if (!ble || !ble->isEnabled()) return false;

    // Add HID service UUID to shared advertising
    ble->addAdvertisingUuid(hal::BleUuid::from16(0x1812));

    advertising_ = true;
    snprintf(statusText_, sizeof(statusText_), "Advertising...");
    LOG_I(TAG, "BLE HID advertising started");
    return true;
}

void BleHidKeyboard::stopAdvertising() {
    if (!advertising_) return;

    auto* ble = hal::getBluetoothControllerInstance();
    if (ble) {
        ble->removeAdvertisingUuid(hal::BleUuid::from16(0x1812));
    }

    advertising_ = false;
    if (!isConnected()) {
        snprintf(statusText_, sizeof(statusText_), "Ready (not advertising)");
    }
    LOG_I(TAG, "BLE HID advertising stopped");
}

bool BleHidKeyboard::isAdvertising() const {
    return advertising_;
}

// ============================================================================
// Connection callbacks
// ============================================================================

void BleHidKeyboard::onConnect(uint16_t connHandle) {
    advertising_ = false;
    snprintf(statusText_, sizeof(statusText_), "Connected");
    LOG_I(TAG, "HID device connected (handle=%d)", connHandle);
}

void BleHidKeyboard::onDisconnect(uint16_t connHandle, int reason) {
    (void)connHandle;
    busy_ = false;
    cancelRequested_ = false;

    snprintf(statusText_, sizeof(statusText_), "Disconnected (reason=%d)", reason);
    LOG_I(TAG, "HID device disconnected (reason=%d)", reason);
}

void BleHidKeyboard::onHostSuspend() {
    LOG_D(TAG, "Host suspended");
}

void BleHidKeyboard::onHostResume() {
    LOG_D(TAG, "Host resumed");
}

// ============================================================================
// Key reports via API
// ============================================================================

bool BleHidKeyboard::sendKeyReport(uint8_t modifier, uint8_t keycode) {
    if (!isConnected() || s_reportHandle == 0) return false;

    auto* ble = hal::getBluetoothControllerInstance();
    if (!ble) return false;

    // Update current report state
    s_currentReport.modifier = modifier;
    s_currentReport.reserved = 0;
    memset(s_currentReport.keycodes, 0, sizeof(s_currentReport.keycodes));
    s_currentReport.keycodes[0] = keycode;

    return ble->sendNotification(
        ble->getConnectionHandle(), s_reportHandle,
        reinterpret_cast<const uint8_t*>(&s_currentReport),
        sizeof(s_currentReport));
}

bool BleHidKeyboard::releaseAllKeys() {
    return sendKeyReport(0, 0);
}

// ============================================================================
// Typing (unchanged - pure application logic)
// ============================================================================

bool BleHidKeyboard::typeString(const char* text, uint16_t delayMs) {
    if (!text || !isConnected()) return false;
    if (busy_) return false;

    busy_ = true;
    cancelRequested_ = false;

    const char* p = text;
    while (*p && !cancelRequested_) {
        uint32_t codepoint;
        int bytes = utf8ToCodepoint(p, &codepoint);
        if (bytes <= 0) {
            p++;
            continue;
        }

        if (codepoint < 128) {
            typeAsciiChar(static_cast<char>(codepoint));
        } else {
            typeUnicodeChar(codepoint);
        }

        p += bytes;

        if (delayMs > 0 && *p) {
            vTaskDelay(pdMS_TO_TICKS(delayMs));
        }
    }

    releaseAllKeys();
    busy_ = false;
    return !cancelRequested_;
}

bool BleHidKeyboard::typeChar(char c) {
    if (!isConnected()) return false;
    return typeAsciiChar(c);
}

bool BleHidKeyboard::isBusy() const {
    return busy_;
}

void BleHidKeyboard::cancel() {
    cancelRequested_ = true;
}

const char* BleHidKeyboard::getStatusText() const {
    return statusText_;
}

void BleHidKeyboard::setUnicodeMethod(UnicodeMethod method) {
    unicodeMethod_ = method;
    saveSettings();
}

bool BleHidKeyboard::typeAsciiChar(char c) {
    KeyMapping mapping = getKeyMapping(c);
    if (mapping.keycode == KeyCode::KEY_NONE) {
        return false;
    }

    if (!sendKeyReport(mapping.modifier, mapping.keycode)) {
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(10));

    if (!releaseAllKeys()) {
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(10));
    return true;
}

bool BleHidKeyboard::typeUnicodeChar(uint32_t codepoint) {
    switch (unicodeMethod_) {
        case UnicodeMethod::WINDOWS:
            return typeWindowsUnicode(codepoint);
        case UnicodeMethod::LINUX:
            return typeLinuxUnicode(codepoint);
        case UnicodeMethod::MACOS:
            return typeMacOsUnicode(codepoint);
        case UnicodeMethod::ASCII_ONLY:
        default:
            return typeAsciiFallback(codepoint);
    }
}

bool BleHidKeyboard::typeWindowsUnicode(uint32_t codepoint) {
    sendKeyReport(Modifier::LEFT_ALT, KeyCode::KEY_NONE);
    vTaskDelay(pdMS_TO_TICKS(20));

    char buf[16];
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)codepoint);

    for (const char* p = buf; *p; p++) {
        uint8_t kp = getNumpadKeycode(*p);
        if (kp != KeyCode::KEY_NONE) {
            sendKeyReport(Modifier::LEFT_ALT, kp);
            vTaskDelay(pdMS_TO_TICKS(20));
            sendKeyReport(Modifier::LEFT_ALT, KeyCode::KEY_NONE);
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }

    releaseAllKeys();
    vTaskDelay(pdMS_TO_TICKS(20));
    return true;
}

bool BleHidKeyboard::typeLinuxUnicode(uint32_t codepoint) {
    sendKeyReport(Modifier::LEFT_CTRL | Modifier::LEFT_SHIFT, KeyCode::KEY_U);
    vTaskDelay(pdMS_TO_TICKS(20));
    releaseAllKeys();
    vTaskDelay(pdMS_TO_TICKS(20));

    char buf[16];
    snprintf(buf, sizeof(buf), "%lx", (unsigned long)codepoint);

    for (const char* p = buf; *p; p++) {
        KeyMapping mapping = getKeyMapping(*p);
        if (mapping.keycode != KeyCode::KEY_NONE) {
            sendKeyReport(mapping.modifier, mapping.keycode);
            vTaskDelay(pdMS_TO_TICKS(20));
            releaseAllKeys();
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }

    sendKeyReport(Modifier::NONE, KeyCode::KEY_ENTER);
    vTaskDelay(pdMS_TO_TICKS(20));
    releaseAllKeys();
    vTaskDelay(pdMS_TO_TICKS(20));

    return true;
}

bool BleHidKeyboard::typeMacOsUnicode(uint32_t codepoint) {
    return typeAsciiFallback(codepoint);
}

bool BleHidKeyboard::typeAsciiFallback(uint32_t codepoint) {
    switch (codepoint) {
        case 0x00E4: typeAsciiChar('a'); typeAsciiChar('e'); return true;
        case 0x00F6: typeAsciiChar('o'); typeAsciiChar('e'); return true;
        case 0x00FC: typeAsciiChar('u'); typeAsciiChar('e'); return true;
        case 0x00C4: typeAsciiChar('A'); typeAsciiChar('e'); return true;
        case 0x00D6: typeAsciiChar('O'); typeAsciiChar('e'); return true;
        case 0x00DC: typeAsciiChar('U'); typeAsciiChar('e'); return true;
        case 0x00DF: typeAsciiChar('s'); typeAsciiChar('s'); return true;
        case 0x20AC: typeAsciiChar('E'); typeAsciiChar('U'); typeAsciiChar('R'); return true;
        default: return true;
    }
}

// ============================================================================
// UTF-8 parsing
// ============================================================================

int BleHidKeyboard::utf8ToCodepoint(const char* utf8, uint32_t* codepoint) {
    if (!utf8 || !codepoint) return 0;

    uint8_t b0 = static_cast<uint8_t>(utf8[0]);

    if ((b0 & 0x80) == 0) {
        *codepoint = b0;
        return 1;
    }

    if ((b0 & 0xE0) == 0xC0) {
        if (!utf8[1]) return 0;
        *codepoint = ((b0 & 0x1F) << 6) | (static_cast<uint8_t>(utf8[1]) & 0x3F);
        return 2;
    }

    if ((b0 & 0xF0) == 0xE0) {
        if (!utf8[1] || !utf8[2]) return 0;
        *codepoint = ((b0 & 0x0F) << 12) |
                     ((static_cast<uint8_t>(utf8[1]) & 0x3F) << 6) |
                     (static_cast<uint8_t>(utf8[2]) & 0x3F);
        return 3;
    }

    if ((b0 & 0xF8) == 0xF0) {
        if (!utf8[1] || !utf8[2] || !utf8[3]) return 0;
        *codepoint = ((b0 & 0x07) << 18) |
                     ((static_cast<uint8_t>(utf8[1]) & 0x3F) << 12) |
                     ((static_cast<uint8_t>(utf8[2]) & 0x3F) << 6) |
                     (static_cast<uint8_t>(utf8[3]) & 0x3F);
        return 4;
    }

    return 0;
}

// ============================================================================
// NVS persistence
// ============================================================================

void BleHidKeyboard::loadSettings() {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) == ESP_OK) {
        uint8_t method = 0;
        if (nvs_get_u8(handle, NVS_KEY_UNICODE, &method) == ESP_OK) {
            unicodeMethod_ = static_cast<UnicodeMethod>(method);
        }
        nvs_close(handle);
    }
}

void BleHidKeyboard::saveSettings() {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_u8(handle, NVS_KEY_UNICODE, static_cast<uint8_t>(unicodeMethod_));
        nvs_commit(handle);
        nvs_close(handle);
    }
}

} // namespace cdc::mod_hid

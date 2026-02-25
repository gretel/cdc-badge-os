#include "mod_hid/BleHidKeyboard.h"
#include "mod_hid/KeyboardLayout.h"
#include "cdc_hal/IBluetoothController.h"
#include "cdc_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include <cstring>
#include <cstdio>

static const char* TAG = "BleHID";

/** \brief NVS namespace and key names for persisted HID settings. */
static constexpr const char* NVS_NAMESPACE = "mod_hid";
static constexpr const char* NVS_KEY_UNICODE = "unicode";

#if defined(CONFIG_BT_ENABLED) && defined(CONFIG_BT_NIMBLE_ENABLED)

#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "services/gatt/ble_svc_gatt.h"
#include "services/gap/ble_svc_gap.h"

namespace cdc::mod_hid {

/** \brief HID report descriptor exported by KeyboardLayout.cpp. */
extern const uint8_t HID_REPORT_MAP[];
extern const size_t HID_REPORT_MAP_SIZE;
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

/** \brief Singleton pointer used by static GAP/GATT callbacks. */
static BleHidKeyboard* s_instance = nullptr;

/** \brief BLE UUID constants for HID service and related characteristics. */
static const ble_uuid16_t HID_SERVICE_UUID = BLE_UUID16_INIT(0x1812);
static const ble_uuid16_t HID_INFO_CHAR_UUID = BLE_UUID16_INIT(0x2A4A);
static const ble_uuid16_t HID_REPORT_MAP_CHAR_UUID = BLE_UUID16_INIT(0x2A4B);
static const ble_uuid16_t HID_REPORT_CHAR_UUID = BLE_UUID16_INIT(0x2A4D);
static const ble_uuid16_t HID_CONTROL_POINT_CHAR_UUID = BLE_UUID16_INIT(0x2A4C);
static const ble_uuid16_t HID_PROTOCOL_MODE_CHAR_UUID = BLE_UUID16_INIT(0x2A4E);

/** \brief Handle assigned by NimBLE for the keyboard input report characteristic. */
static uint16_t s_reportHandle = 0;

/**
 * \brief Handles GATT read/write access for HID characteristics.
 * \param connHandle Active BLE connection handle.
 * \param attrHandle Accessed attribute handle.
 * \param ctxt Access context containing operation and payload buffers.
 * \param arg Optional callback argument.
 * \return NimBLE ATT status code.
 */
static int hidGattAccess(uint16_t connHandle, uint16_t attrHandle,
                         struct ble_gatt_access_ctxt* ctxt, void* arg);

/** \brief HID GATT characteristic table. */
/** \brief Field order must match NimBLE struct definition exactly. */
static struct ble_gatt_chr_def s_hidChars[] = {
    {
        // HID Information
        .uuid = &HID_INFO_CHAR_UUID.u,
        .access_cb = hidGattAccess,
        .arg = nullptr,
        .descriptors = nullptr,
        .flags = BLE_GATT_CHR_F_READ,
        .min_key_size = 0,
        .val_handle = nullptr,
    },
    {
        // Report Map
        .uuid = &HID_REPORT_MAP_CHAR_UUID.u,
        .access_cb = hidGattAccess,
        .arg = nullptr,
        .descriptors = nullptr,
        .flags = BLE_GATT_CHR_F_READ,
        .min_key_size = 0,
        .val_handle = nullptr,
    },
    {
        // Keyboard Input Report
        .uuid = &HID_REPORT_CHAR_UUID.u,
        .access_cb = hidGattAccess,
        .arg = nullptr,
        .descriptors = nullptr,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        .min_key_size = 0,
        .val_handle = &s_reportHandle,
    },
    {
        // HID Control Point
        .uuid = &HID_CONTROL_POINT_CHAR_UUID.u,
        .access_cb = hidGattAccess,
        .arg = nullptr,
        .descriptors = nullptr,
        .flags = BLE_GATT_CHR_F_WRITE_NO_RSP,
        .min_key_size = 0,
        .val_handle = nullptr,
    },
    {
        // Protocol Mode
        .uuid = &HID_PROTOCOL_MODE_CHAR_UUID.u,
        .access_cb = hidGattAccess,
        .arg = nullptr,
        .descriptors = nullptr,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE_NO_RSP,
        .min_key_size = 0,
        .val_handle = nullptr,
    },
    {
        // End marker
        .uuid = nullptr,
        .access_cb = nullptr,
        .arg = nullptr,
        .descriptors = nullptr,
        .flags = 0,
        .min_key_size = 0,
        .val_handle = nullptr,
    }
};

static struct ble_gatt_svc_def s_hidSvc[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &HID_SERVICE_UUID.u,
        .includes = nullptr,
        .characteristics = s_hidChars,
    },
    {
        // End marker
        .type = 0,
        .uuid = nullptr,
        .includes = nullptr,
        .characteristics = nullptr,
    }
};

/**
 * \brief Handles BLE GAP events for connection and advertising lifecycle.
 * \param event NimBLE GAP event record.
 * \param arg Optional callback argument.
 * \return Always returns 0.
 */
static int hidGapEventHandler(struct ble_gap_event* event, void* arg);

/** \brief HID protocol mode (`0` boot, `1` report). */
static uint8_t s_protocolMode = 1;

/** \brief Last transmitted keyboard report state. */
static KeyboardReport s_currentReport = {};

/**
 * \brief Handles GATT read/write access for HID service characteristics.
 * \param connHandle Active BLE connection handle.
 * \param attrHandle Accessed attribute handle.
 * \param ctxt Access context containing operation and mbuf payload.
 * \param arg Optional callback argument.
 * \return NimBLE ATT status code.
 */
static int hidGattAccess(uint16_t connHandle, uint16_t attrHandle,
                         struct ble_gatt_access_ctxt* ctxt, void* arg) {
    (void)connHandle;
    (void)attrHandle;
    (void)arg;

    const ble_uuid_t* uuid = ctxt->chr->uuid;

    if (ble_uuid_cmp(uuid, &HID_INFO_CHAR_UUID.u) == 0) {
        // HID Information
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            os_mbuf_append(ctxt->om, HID_INFO, sizeof(HID_INFO));
            return 0;
        }
    } else if (ble_uuid_cmp(uuid, &HID_REPORT_MAP_CHAR_UUID.u) == 0) {
        // Report Map
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            os_mbuf_append(ctxt->om, getHidReportMap(), getHidReportMapSize());
            return 0;
        }
    } else if (ble_uuid_cmp(uuid, &HID_REPORT_CHAR_UUID.u) == 0) {
        // Keyboard Input Report
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            os_mbuf_append(ctxt->om, &s_currentReport, sizeof(s_currentReport));
            return 0;
        }
    } else if (ble_uuid_cmp(uuid, &HID_CONTROL_POINT_CHAR_UUID.u) == 0) {
        // HID Control Point (Suspend/Exit Suspend)
        if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            uint8_t cmd;
            if (OS_MBUF_PKTLEN(ctxt->om) >= 1) {
                os_mbuf_copydata(ctxt->om, 0, 1, &cmd);
                if (cmd == 0 && s_instance) {
                    s_instance->onHostSuspend();
                } else if (cmd == 1 && s_instance) {
                    s_instance->onHostResume();
                }
            }
            return 0;
        }
    } else if (ble_uuid_cmp(uuid, &HID_PROTOCOL_MODE_CHAR_UUID.u) == 0) {
        // Protocol Mode
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            os_mbuf_append(ctxt->om, &s_protocolMode, 1);
            return 0;
        } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            if (OS_MBUF_PKTLEN(ctxt->om) >= 1) {
                os_mbuf_copydata(ctxt->om, 0, 1, &s_protocolMode);
            }
            return 0;
        }
    }

    return BLE_ATT_ERR_UNLIKELY;
}

/**
 * \brief Returns the module singleton instance.
 * \return Reference to the singleton keyboard service.
 */
BleHidKeyboard& BleHidKeyboard::instance() {
    static BleHidKeyboard inst;
    return inst;
}

/**
 * \brief Initializes BLE HID service registration and local settings.
 * \return `true` on success, otherwise `false`.
 */
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

    // Register HID GATT service
    int rc = ble_gatts_count_cfg(s_hidSvc);
    if (rc != 0) {
        LOG_E(TAG, "ble_gatts_count_cfg failed: %d", rc);
        return false;
    }

    rc = ble_gatts_add_svcs(s_hidSvc);
    if (rc != 0) {
        LOG_E(TAG, "ble_gatts_add_svcs failed: %d", rc);
        return false;
    }

    initialized_ = true;
    snprintf(statusText_, sizeof(statusText_), "Ready (not advertising)");
    LOG_I(TAG, "BLE HID Keyboard initialized");
    return true;
}

/**
 * \brief Stops advertising and resets runtime state.
 */
void BleHidKeyboard::deinit() {
    if (!initialized_) return;

    stopAdvertising();
    s_instance = nullptr;
    initialized_ = false;
    snprintf(statusText_, sizeof(statusText_), "Not initialized");
    LOG_I(TAG, "BLE HID Keyboard deinitialized");
}

/**
 * \brief Reports whether a host is currently connected.
 * \return `true` when connected, otherwise `false`.
 */
bool BleHidKeyboard::isConnected() const {
    return connHandle_ != 0xFFFF;
}

/**
 * \brief Types a UTF-8 string over HID, optionally adding delay between characters.
 * \param text UTF-8 text to emit.
 * \param delayMs Inter-character delay in milliseconds.
 * \return `true` when completed without cancellation, otherwise `false`.
 */
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

/**
 * \brief Types a single ASCII character via keyboard HID report.
 * \param c ASCII character to type.
 * \return `true` on success, otherwise `false`.
 */
bool BleHidKeyboard::typeChar(char c) {
    if (!isConnected()) return false;
    return typeAsciiChar(c);
}

/**
 * \brief Reports whether the keyboard is currently typing.
 * \return `true` if typing is active, otherwise `false`.
 */
bool BleHidKeyboard::isBusy() const {
    return busy_;
}

/**
 * \brief Requests cancellation of the active typing operation.
 */
void BleHidKeyboard::cancel() {
    cancelRequested_ = true;
}

/**
 * \brief Returns human-readable module status text.
 * \return Null-terminated status string.
 */
const char* BleHidKeyboard::getStatusText() const {
    return statusText_;
}

/**
 * \brief Updates the Unicode input strategy and persists it.
 * \param method Unicode input strategy.
 */
void BleHidKeyboard::setUnicodeMethod(UnicodeMethod method) {
    unicodeMethod_ = method;
    saveSettings();
}

/**
 * \brief Starts BLE advertising with HID keyboard appearance and UUID.
 * \return `true` on success, otherwise `false`.
 */
bool BleHidKeyboard::startAdvertising() {
    if (!initialized_ || advertising_) return advertising_;

    auto* ble = hal::getBluetoothControllerInstance();
    if (!ble || !ble->isEnabled()) return false;

    // Set device name for HID
    ble->setDeviceName("CDC Badge Keyboard");

    // Build advertising data
    struct ble_hs_adv_fields fields = {};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    // Include HID service UUID in advertising
    static ble_uuid16_t hidUuid = BLE_UUID16_INIT(0x1812);
    fields.uuids16 = &hidUuid;
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    // Device name
    const char* name = ble->getDeviceName();
    fields.name = reinterpret_cast<const uint8_t*>(name);
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    // Device appearance (keyboard)
    fields.appearance = 0x03C1;  // HID Keyboard
    fields.appearance_is_present = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        LOG_E(TAG, "Failed to set adv fields: %d", rc);
        return false;
    }

    // Start advertising
    struct ble_gap_adv_params advParams = {};
    advParams.conn_mode = BLE_GAP_CONN_MODE_UND;
    advParams.disc_mode = BLE_GAP_DISC_MODE_GEN;
    advParams.itvl_min = BLE_GAP_ADV_FAST_INTERVAL1_MIN;
    advParams.itvl_max = BLE_GAP_ADV_FAST_INTERVAL1_MAX;

    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, nullptr, BLE_HS_FOREVER,
                           &advParams, hidGapEventHandler, nullptr);
    if (rc != 0) {
        LOG_E(TAG, "Failed to start advertising: %d", rc);
        return false;
    }

    advertising_ = true;
    snprintf(statusText_, sizeof(statusText_), "Advertising...");
    LOG_I(TAG, "BLE HID advertising started");
    return true;
}

/**
 * \brief Stops active BLE advertising for the HID profile.
 */
void BleHidKeyboard::stopAdvertising() {
    if (!advertising_) return;

    ble_gap_adv_stop();
    advertising_ = false;
    if (!isConnected()) {
        snprintf(statusText_, sizeof(statusText_), "Ready (not advertising)");
    }
    LOG_I(TAG, "BLE HID advertising stopped");
}

/**
 * \brief Reports whether HID advertising is currently active.
 * \return `true` when advertising, otherwise `false`.
 */
bool BleHidKeyboard::isAdvertising() const {
    return advertising_;
}

/**
 * \brief Handles successful HID connection establishment.
 * \param connHandle NimBLE connection handle.
 */
void BleHidKeyboard::onConnect(uint16_t connHandle) {
    connHandle_ = connHandle;
    advertising_ = false;

    // Get connected device name
    struct ble_gap_conn_desc desc;
    if (ble_gap_conn_find(connHandle, &desc) == 0) {
        snprintf(statusText_, sizeof(statusText_), "Connected");
    } else {
        snprintf(statusText_, sizeof(statusText_), "Connected");
    }

    LOG_I(TAG, "HID device connected (handle=%d)", connHandle);
}

/**
 * \brief Handles HID disconnect and optionally restarts advertising.
 * \param connHandle NimBLE connection handle.
 * \param reason Disconnect reason code.
 */
void BleHidKeyboard::onDisconnect(uint16_t connHandle, int reason) {
    (void)connHandle;
    connHandle_ = 0xFFFF;
    busy_ = false;
    cancelRequested_ = false;

    snprintf(statusText_, sizeof(statusText_), "Disconnected (reason=%d)", reason);
    LOG_I(TAG, "HID device disconnected (reason=%d)", reason);

    // Auto-restart advertising
    if (initialized_) {
        vTaskDelay(pdMS_TO_TICKS(500));
        startAdvertising();
    }
}

/**
 * \brief Called when the host sends HID suspend.
 */
void BleHidKeyboard::onHostSuspend() {
    LOG_D(TAG, "Host suspended");
}

/**
 * \brief Called when the host exits HID suspend state.
 */
void BleHidKeyboard::onHostResume() {
    LOG_D(TAG, "Host resumed");
}

/**
 * \brief Sends one keyboard input report notification.
 * \param modifier Modifier bitmask.
 * \param keycode HID keycode in slot 0.
 * \return `true` on success, otherwise `false`.
 */
bool BleHidKeyboard::sendKeyReport(uint8_t modifier, uint8_t keycode) {
    if (!isConnected() || s_reportHandle == 0) return false;

    // Update current report state
    s_currentReport.modifier = modifier;
    s_currentReport.reserved = 0;
    memset(s_currentReport.keycodes, 0, sizeof(s_currentReport.keycodes));
    s_currentReport.keycodes[0] = keycode;

    // Build notification data
    struct os_mbuf* om = ble_hs_mbuf_from_flat(&s_currentReport, sizeof(s_currentReport));
    if (!om) {
        LOG_E(TAG, "Failed to allocate mbuf");
        return false;
    }

    // Send notification
    int rc = ble_gatts_notify_custom(connHandle_, s_reportHandle, om);
    if (rc != 0) {
        LOG_E(TAG, "Failed to send key report: %d", rc);
        return false;
    }

    return true;
}

/**
 * \brief Releases all pressed keys.
 * \return `true` on success, otherwise `false`.
 */
bool BleHidKeyboard::releaseAllKeys() {
    return sendKeyReport(0, 0);
}

/**
 * \brief Types a single ASCII character using layout mapping.
 * \param c ASCII character.
 * \return `true` on success, otherwise `false`.
 */
bool BleHidKeyboard::typeAsciiChar(char c) {
    KeyMapping mapping = getKeyMapping(c);
    if (mapping.keycode == KeyCode::KEY_NONE) {
        return false;
    }

    // Press key
    if (!sendKeyReport(mapping.modifier, mapping.keycode)) {
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(10));

    // Release key
    if (!releaseAllKeys()) {
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(10));
    return true;
}

/**
 * \brief Types a Unicode codepoint according to the configured method.
 * \param codepoint Unicode codepoint value.
 * \return `true` on success, otherwise `false`.
 */
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

/**
 * \brief Types a Unicode codepoint via Windows Alt+numpad sequence.
 * \param codepoint Unicode codepoint value.
 * \return `true` on success, otherwise `false`.
 */
bool BleHidKeyboard::typeWindowsUnicode(uint32_t codepoint) {
    // Alt + numpad decimal code
    // Hold Alt, type decimal codepoint on numpad, release Alt

    // Hold Left Alt
    sendKeyReport(Modifier::LEFT_ALT, KeyCode::KEY_NONE);
    vTaskDelay(pdMS_TO_TICKS(20));

    // Convert codepoint to decimal string
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)codepoint);

    // Type each digit on numpad
    for (const char* p = buf; *p; p++) {
        uint8_t kp = getNumpadKeycode(*p);
        if (kp != KeyCode::KEY_NONE) {
            sendKeyReport(Modifier::LEFT_ALT, kp);
            vTaskDelay(pdMS_TO_TICKS(20));
            sendKeyReport(Modifier::LEFT_ALT, KeyCode::KEY_NONE);
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }

    // Release Alt
    releaseAllKeys();
    vTaskDelay(pdMS_TO_TICKS(20));
    return true;
}

/**
 * \brief Types a Unicode codepoint via Linux Ctrl+Shift+U input method.
 * \param codepoint Unicode codepoint value.
 * \return `true` on success, otherwise `false`.
 */
bool BleHidKeyboard::typeLinuxUnicode(uint32_t codepoint) {
    // Ctrl+Shift+U, hex digits, Enter/Space

    // Press Ctrl+Shift+U
    sendKeyReport(Modifier::LEFT_CTRL | Modifier::LEFT_SHIFT, KeyCode::KEY_U);
    vTaskDelay(pdMS_TO_TICKS(20));
    releaseAllKeys();
    vTaskDelay(pdMS_TO_TICKS(20));

    // Type hex digits
    char buf[16];
    snprintf(buf, sizeof(buf), "%lx", (unsigned long)codepoint);

    for (const char* p = buf; *p; p++) {
        char c = *p;
        // Convert to lowercase letter or digit
        KeyMapping mapping = getKeyMapping(c);
        if (mapping.keycode != KeyCode::KEY_NONE) {
            sendKeyReport(mapping.modifier, mapping.keycode);
            vTaskDelay(pdMS_TO_TICKS(20));
            releaseAllKeys();
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }

    // Press Enter to confirm
    sendKeyReport(Modifier::NONE, KeyCode::KEY_ENTER);
    vTaskDelay(pdMS_TO_TICKS(20));
    releaseAllKeys();
    vTaskDelay(pdMS_TO_TICKS(20));

    return true;
}

/**
 * \brief Handles Unicode typing on macOS by falling back to ASCII replacement.
 * \param codepoint Unicode codepoint value.
 * \return `true` on success, otherwise `false`.
 */
bool BleHidKeyboard::typeMacOsUnicode(uint32_t codepoint) {
    // macOS Unicode input is limited without special keyboard layouts
    // Fall back to ASCII replacement
    return typeAsciiFallback(codepoint);
}

/**
 * \brief Replaces selected Unicode characters with ASCII approximations.
 * \param codepoint Unicode codepoint value.
 * \return `true` when handled or intentionally skipped, otherwise `false`.
 */
bool BleHidKeyboard::typeAsciiFallback(uint32_t codepoint) {
    // Common German umlaut replacements
    switch (codepoint) {
        case 0x00E4: // ae
            typeAsciiChar('a');
            typeAsciiChar('e');
            return true;
        case 0x00F6: // oe
            typeAsciiChar('o');
            typeAsciiChar('e');
            return true;
        case 0x00FC: // ue
            typeAsciiChar('u');
            typeAsciiChar('e');
            return true;
        case 0x00C4: // Ae
            typeAsciiChar('A');
            typeAsciiChar('e');
            return true;
        case 0x00D6: // Oe
            typeAsciiChar('O');
            typeAsciiChar('e');
            return true;
        case 0x00DC: // Ue
            typeAsciiChar('U');
            typeAsciiChar('e');
            return true;
        case 0x00DF: // ss (eszett)
            typeAsciiChar('s');
            typeAsciiChar('s');
            return true;
        case 0x20AC: // Euro sign -> EUR
            typeAsciiChar('E');
            typeAsciiChar('U');
            typeAsciiChar('R');
            return true;
        default:
            // Skip unknown characters
            return true;
    }
}

/**
 * \brief Decodes one UTF-8 sequence into a Unicode codepoint.
 * \param utf8 Pointer to UTF-8 input bytes.
 * \param codepoint Destination for decoded codepoint.
 * \return Number of consumed bytes, or `0` on invalid/incomplete input.
 */
int BleHidKeyboard::utf8ToCodepoint(const char* utf8, uint32_t* codepoint) {
    if (!utf8 || !codepoint) return 0;

    uint8_t b0 = static_cast<uint8_t>(utf8[0]);

    // Single byte (ASCII)
    if ((b0 & 0x80) == 0) {
        *codepoint = b0;
        return 1;
    }

    // Two bytes
    if ((b0 & 0xE0) == 0xC0) {
        if (!utf8[1]) return 0;
        *codepoint = ((b0 & 0x1F) << 6) | (static_cast<uint8_t>(utf8[1]) & 0x3F);
        return 2;
    }

    // Three bytes
    if ((b0 & 0xF0) == 0xE0) {
        if (!utf8[1] || !utf8[2]) return 0;
        *codepoint = ((b0 & 0x0F) << 12) |
                     ((static_cast<uint8_t>(utf8[1]) & 0x3F) << 6) |
                     (static_cast<uint8_t>(utf8[2]) & 0x3F);
        return 3;
    }

    // Four bytes
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

/**
 * \brief Loads persisted HID settings from NVS.
 */
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

/**
 * \brief Saves current HID settings to NVS.
 */
void BleHidKeyboard::saveSettings() {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_u8(handle, NVS_KEY_UNICODE, static_cast<uint8_t>(unicodeMethod_));
        nvs_commit(handle);
        nvs_close(handle);
    }
}

/**
 * \brief Handles BLE GAP events for HID connection and advertising state.
 * \param event NimBLE GAP event data.
 * \param arg Optional callback argument.
 * \return Always returns `0`.
 */
static int hidGapEventHandler(struct ble_gap_event* event, void* arg) {
    (void)arg;
    if (!s_instance) return 0;

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                s_instance->onConnect(event->connect.conn_handle);
            } else {
                LOG_W(TAG, "HID connection failed: %d", event->connect.status);
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            s_instance->onDisconnect(event->disconnect.conn.conn_handle,
                                     event->disconnect.reason);
            break;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            LOG_D(TAG, "Advertising complete");
            break;

        case BLE_GAP_EVENT_SUBSCRIBE:
            LOG_I(TAG, "Subscription event: attr_handle=%d, cur_notify=%d",
                  event->subscribe.attr_handle, event->subscribe.cur_notify);
            break;

        case BLE_GAP_EVENT_MTU:
            LOG_I(TAG, "MTU updated: %d", event->mtu.value);
            break;

        default:
            break;
    }

    return 0;
}

} // namespace cdc::mod_hid

#else // NimBLE not enabled - stub implementation

namespace cdc::mod_hid {

/**
 * \brief Returns the module singleton instance (stub build).
 * \return Reference to singleton stub instance.
 */
BleHidKeyboard& BleHidKeyboard::instance() {
    static BleHidKeyboard inst;
    return inst;
}

/**
 * \brief Stub initialization when NimBLE support is not available.
 * \return Always returns `false`.
 */
bool BleHidKeyboard::init() {
    LOG_W(TAG, "BLE HID disabled (NimBLE not configured)");
    snprintf(statusText_, sizeof(statusText_), "BLE not available");
    return false;
}

/** \brief Stub deinitialization (no-op). */
void BleHidKeyboard::deinit() {}
/** \brief Stub connectivity state (always `false`). */
bool BleHidKeyboard::isConnected() const { return false; }
/** \brief Stub typing operation (always `false`). */
bool BleHidKeyboard::typeString(const char*, uint16_t) { return false; }
/** \brief Stub single-character typing (always `false`). */
bool BleHidKeyboard::typeChar(char) { return false; }
/** \brief Stub busy state (always `false`). */
bool BleHidKeyboard::isBusy() const { return false; }
/** \brief Stub cancel operation (no-op). */
void BleHidKeyboard::cancel() {}
/** \brief Returns current stub status text. */
const char* BleHidKeyboard::getStatusText() const { return statusText_; }
/** \brief Stub unicode method setter (no-op). */
void BleHidKeyboard::setUnicodeMethod(UnicodeMethod) {}
/** \brief Stub advertising start (always `false`). */
bool BleHidKeyboard::startAdvertising() { return false; }
/** \brief Stub advertising stop (no-op). */
void BleHidKeyboard::stopAdvertising() {}
/** \brief Stub advertising state (always `false`). */
bool BleHidKeyboard::isAdvertising() const { return false; }
/** \brief Stub connect callback (no-op). */
void BleHidKeyboard::onConnect(uint16_t) {}
/** \brief Stub disconnect callback (no-op). */
void BleHidKeyboard::onDisconnect(uint16_t, int) {}
/** \brief Stub suspend callback (no-op). */
void BleHidKeyboard::onHostSuspend() {}
/** \brief Stub resume callback (no-op). */
void BleHidKeyboard::onHostResume() {}
/** \brief Stub settings load (no-op). */
void BleHidKeyboard::loadSettings() {}
/** \brief Stub settings save (no-op). */
void BleHidKeyboard::saveSettings() {}

} // namespace cdc::mod_hid

#endif // CONFIG_BT_ENABLED && CONFIG_BT_NIMBLE_ENABLED

// USB HID Module - Composite Device Implementation
// CDC + optional FIDO HID + optional Keyboard HID
// Interfaces are conditionally included based on FEATURE_FIDO2_USB_USB and FEATURE_USB_KEYBOARD
//
// Uses raw TinyUSB - the TinyUSB DWC2 driver handles PHY initialization

#include "usb_hid.h"
#include "usb_descriptors.h"
#include "cdc_log.h"
#include "feature_flags.h"

#include "esp_err.h"
#include "esp_idf_version.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0) && defined(CONFIG_SOC_USB_OTG_SUPPORTED) && CONFIG_SOC_USB_OTG_SUPPORTED
#include "esp_private/usb_phy.h"
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

extern "C" {
#include "tusb.h"
#include "class/cdc/cdc.h"
#include "class/hid/hid_device.h"
#if FEATURE_GPG_CCID
#include "device/usbd_pvt.h"
#endif
}

#include <string.h>

#if FEATURE_GPG_CCID
#include "ccid.h"
#include "openpgp.h"
#endif

// ============================================================================
// State
// ============================================================================

static bool g_usb_initialized = false;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0) && defined(CONFIG_SOC_USB_OTG_SUPPORTED) && CONFIG_SOC_USB_OTG_SUPPORTED
static usb_phy_handle_t g_usb_phy = nullptr;

static bool usb_phy_init_once(void) {
    if (g_usb_phy) return true;

    usb_phy_config_t phy_conf = {
        .controller = USB_PHY_CTRL_OTG,
        .target = USB_PHY_TARGET_INT,
        .otg_mode = USB_OTG_MODE_DEVICE,
        // Auto-detect speed to avoid timing/race issues on some hosts
        .otg_speed = USB_PHY_SPEED_UNDEFINED,
        .ext_io_conf = nullptr,
        .otg_io_conf = nullptr,
    };

    esp_err_t err = usb_new_phy(&phy_conf, &g_usb_phy);
    if (err != ESP_OK) {
        LOG_E("USB", "usb_new_phy failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}
#endif

#if FEATURE_FIDO2_USB
// FIDO packet queue (64 bytes each)
// Using FreeRTOS queue for proper flow control - ISR can send without blocking,
// consumer can wait with timeout if queue is empty.
#define FIDO_QUEUE_SIZE 8
static QueueHandle_t g_fido_queue = nullptr;

// Packet structure for queue
typedef struct {
    uint8_t data[64];
} fido_packet_t;

// Semaphore for TX completion synchronization
// Released by tud_hid_report_complete_cb when host has received the report
static SemaphoreHandle_t g_fido_tx_sem = nullptr;
#endif

// ============================================================================
// USB Descriptors (only used when NOT in ROM console mode)
// ============================================================================

#if !CONFIG_ESP_CONSOLE_USB_CDC

// Device Descriptor
static const tusb_desc_device_t device_descriptor = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = USB_BCD,
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = USB_VID,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0100,
    .iManufacturer      = STR_MANUFACTURER,
    .iProduct           = STR_PRODUCT,
    .iSerialNumber      = STR_SERIAL,
    .bNumConfigurations = 1
};

// HID Report Descriptors
#if FEATURE_FIDO2_USB
static const uint8_t fido_report_desc[] = {
    TUD_HID_REPORT_DESC_FIDO_U2F(EP_FIDO_SIZE)
};
#endif

#if FEATURE_USB_KEYBOARD
static const uint8_t keyboard_report_desc[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(REPORT_ID_KEYBOARD))
};
#endif

// ============================================================================
// CCID SmartCard Descriptor (54 bytes class descriptor + endpoints)
// ============================================================================
// CCID Class Descriptor (54 bytes) - USB CCID 1.1 specification
// Using Gemalto VID/PID for libccid compatibility (see plan.md)
#define TUD_CCID_DESC_LEN  54
#define TUD_CCID_TOTAL_LEN (9 + TUD_CCID_DESC_LEN + 7 + 7)  // Interface + Class + 2x Endpoint

#if FEATURE_GPG_CCID
// CCID Class Descriptor Macro
#define TUD_CCID_DESCRIPTOR(_itfnum, _stridx, _epout, _epin, _epsize) \
    /* Interface */ \
    9, TUSB_DESC_INTERFACE, _itfnum, 0, 2, TUSB_CLASS_SMART_CARD, 0, 0, _stridx, \
    /* CCID Class Descriptor (54 bytes) */ \
    TUD_CCID_DESC_LEN, 0x21, /* bDescriptorType: CCID Functional */ \
    0x10, 0x01,              /* bcdCCID: CCID 1.1 */ \
    0x00,                    /* bMaxSlotIndex: 1 slot */ \
    0x07,                    /* bVoltageSupport: 5V, 3V, 1.8V */ \
    0x02, 0x00, 0x00, 0x00,  /* dwProtocols: T=1 */ \
    0xA0, 0x0F, 0x00, 0x00,  /* dwDefaultClock: 4000 kHz */ \
    0xA0, 0x0F, 0x00, 0x00,  /* dwMaximumClock: 4000 kHz */ \
    0x00,                    /* bNumClockSupported */ \
    0x00, 0x2A, 0x00, 0x00,  /* dwDataRate: 10752 bps */ \
    0x00, 0x2A, 0x00, 0x00,  /* dwMaxDataRate: 10752 bps */ \
    0x00,                    /* bNumDataRatesSupported */ \
    0xFE, 0x00, 0x00, 0x00,  /* dwMaxIFSD: 254 */ \
    0x00, 0x00, 0x00, 0x00,  /* dwSynchProtocols: none */ \
    0x00, 0x00, 0x00, 0x00,  /* dwMechanical: none */ \
    0xFE, 0x00, 0x04, 0x00,  /* dwFeatures: auto config/activation/voltage/clock/baud/negotiation/PPS, extended APDU */ \
    0x0F, 0x01, 0x00, 0x00,  /* dwMaxCCIDMessageLength: 271 */ \
    0xFF,                    /* bClassGetResponse */ \
    0xFF,                    /* bClassEnvelope */ \
    0x00, 0x00,              /* wLcdLayout: none */ \
    0x00,                    /* bPINSupport: none */ \
    0x01,                    /* bMaxCCIDBusySlots: 1 */ \
    /* Bulk OUT Endpoint */ \
    7, TUSB_DESC_ENDPOINT, _epout, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0, \
    /* Bulk IN Endpoint */ \
    7, TUSB_DESC_ENDPOINT, _epin, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0
#define CCID_CONFIG_LEN TUD_CCID_TOTAL_LEN
#else
#define CCID_CONFIG_LEN 0
#endif

// Configuration Descriptor
// Calculate total length based on enabled features
#define CONFIG_TOTAL_LEN ( \
    TUD_CONFIG_DESC_LEN + \
    TUD_CDC_DESC_LEN + \
    (FEATURE_FIDO2_USB ? TUD_HID_INOUT_DESC_LEN : 0) + \
    (FEATURE_USB_KEYBOARD ? TUD_HID_DESC_LEN : 0) + \
    CCID_CONFIG_LEN \
)

static const uint8_t config_descriptor[] = {
    // Config: variable number of interfaces
    TUD_CONFIG_DESCRIPTOR(1, ITF_COUNT, 0, CONFIG_TOTAL_LEN, 0, 100),

    // CDC ACM: Interface 0 + 1 (always present for serial console)
    TUD_CDC_DESCRIPTOR(ITF_CDC, STR_CDC, EP_CDC_NOTIF, EP_CDC_NOTIF_SIZE,
                       EP_CDC_OUT, EP_CDC_IN, EP_CDC_SIZE),

#if FEATURE_FIDO2_USB
    // FIDO HID: Interface 2 (IN + OUT, no Report ID)
    TUD_HID_INOUT_DESCRIPTOR(ITF_FIDO, STR_FIDO, HID_ITF_PROTOCOL_NONE,
                             sizeof(fido_report_desc), EP_FIDO_OUT, EP_FIDO_IN,
                             EP_FIDO_SIZE, 5),
#endif

#if FEATURE_USB_KEYBOARD
    // Keyboard HID: Interface (IN only, with Report ID)
    TUD_HID_DESCRIPTOR(ITF_KEYBOARD, STR_KEYBOARD, HID_ITF_PROTOCOL_KEYBOARD,
                       sizeof(keyboard_report_desc), EP_KEYBOARD_IN,
                       EP_KEYBOARD_SIZE, 10),
#endif

#if FEATURE_GPG_CCID
    // CCID SmartCard Interface
    TUD_CCID_DESCRIPTOR(ITF_CCID, STR_CCID, EP_CCID_OUT, EP_CCID_IN, EP_CCID_SIZE),
#endif
};

// String Descriptors
static const char* string_descriptors[] = {
    (const char[]){0x09, 0x04},  // 0: Language (English US)
    "CDC",                        // 1: Manufacturer
    "BadgeV1",                    // 2: Product
    "000001",                     // 3: Serial
    "CDC Serial",                 // 4: CDC Interface
#if FEATURE_FIDO2_USB
    "FIDO2",                      // 5: FIDO Interface
#endif
#if FEATURE_USB_KEYBOARD
    "Keyboard",                   // 6: Keyboard Interface (or 5 if no FIDO)
#endif
#if FEATURE_GPG_CCID
    "OpenPGP SmartCard",          // CCID Interface string
#endif
};

#endif // !CONFIG_ESP_CONSOLE_USB_CDC

// ============================================================================
// TinyUSB Callbacks
// ============================================================================

extern "C" {

// Descriptor callbacks
// When CONFIG_ESP_CONSOLE_USB_CDC is enabled, ROM provides its own TinyUSB init,
// but the managed component still needs these callbacks for linking.
// Return NULL in ROM mode since ROM handles everything.

uint8_t const* tud_descriptor_device_cb(void) {
#if CONFIG_ESP_CONSOLE_USB_CDC
    return nullptr;  // ROM handles this
#else
    return (uint8_t const*)&device_descriptor;
#endif
}

uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
#if CONFIG_ESP_CONSOLE_USB_CDC
    return nullptr;  // ROM handles this
#else
    return config_descriptor;
#endif
}

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
#if CONFIG_ESP_CONSOLE_USB_CDC
    (void)index;
    return nullptr;  // ROM handles this
#else
    static uint16_t str_desc[32];

    if (index >= STR_COUNT) return nullptr;

    const char* str = string_descriptors[index];
    if (index == 0) {
        str_desc[0] = (TUSB_DESC_STRING << 8) | 4;
        str_desc[1] = 0x0409;
        return str_desc;
    }

    uint8_t len = strlen(str);
    if (len > 31) len = 31;
    str_desc[0] = (TUSB_DESC_STRING << 8) | (2 + 2 * len);
    for (uint8_t i = 0; i < len; i++) {
        str_desc[1 + i] = str[i];
    }
    return str_desc;
#endif
}

// HID callbacks - always defined for linking, but only functional when:
// 1. NOT using ROM's CDC console (CONFIG_ESP_CONSOLE_USB_CDC disabled)
// 2. AND HID features are enabled
uint8_t const* tud_hid_descriptor_report_cb(uint8_t instance) {
#if !CONFIG_ESP_CONSOLE_USB_CDC
#if FEATURE_FIDO2_USB
    if (instance == USB_HID_INSTANCE_FIDO) {
        return fido_report_desc;
    }
#endif
#if FEATURE_USB_KEYBOARD
    if (instance == USB_HID_INSTANCE_KEYBOARD) {
        return keyboard_report_desc;
    }
#endif
#else
    (void)instance;
#endif
    return nullptr;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                               hid_report_type_t report_type,
                               uint8_t* buffer, uint16_t reqlen) {
    (void)instance; (void)report_id; (void)report_type;
    (void)buffer; (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type,
                           uint8_t const* buffer, uint16_t bufsize) {
    (void)report_id; (void)report_type;

#if !CONFIG_ESP_CONSOLE_USB_CDC && FEATURE_FIDO2_USB
    // FIDO HID: receive 64-byte packets
    if (instance == USB_HID_INSTANCE_FIDO && bufsize == 64 && g_fido_queue) {
        fido_packet_t packet;
        memcpy(packet.data, buffer, 64);

        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        if (xQueueSendFromISR(g_fido_queue, &packet, &xHigherPriorityTaskWoken) != pdTRUE) {
            LOG_W("USB", "FIDO RX queue full");
        }
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
#endif
    // Keyboard HID: LED status (ignore)
    (void)instance;
    (void)buffer;
    (void)bufsize;
}

// Called when a HID report was successfully sent to the host
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report, uint16_t len) {
    (void)report;
    (void)len;

#if !CONFIG_ESP_CONSOLE_USB_CDC && FEATURE_FIDO2_USB
    // Signal FIDO TX completion so next packet can be sent
    if (instance == USB_HID_INSTANCE_FIDO && g_fido_tx_sem) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(g_fido_tx_sem, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
#else
    (void)instance;
#endif
}

} // extern "C"

// ============================================================================
// USB Task
// ============================================================================

static TaskHandle_t g_usb_task = nullptr;

static void usb_device_task(void* arg) {
    (void)arg;
    while (1) {
        tud_task();
#if FEATURE_GPG_CCID
        usb_ccid::task();
#endif
        vTaskDelay(1);
    }
}

// ============================================================================
// Public API
// ============================================================================

bool usb_hid_init(void) {
    if (g_usb_initialized) return true;

    LOG_I("USB", "Initializing USB...");

#if FEATURE_FIDO2_USB
    // Create FIDO RX queue
    g_fido_queue = xQueueCreate(FIDO_QUEUE_SIZE, sizeof(fido_packet_t));
    if (!g_fido_queue) {
        LOG_E("USB", "Failed to create FIDO queue");
        return false;
    }

    // Create FIDO TX completion semaphore (binary semaphore, starts available)
    g_fido_tx_sem = xSemaphoreCreateBinary();
    if (!g_fido_tx_sem) {
        LOG_E("USB", "Failed to create FIDO TX semaphore");
        return false;
    }
    xSemaphoreGive(g_fido_tx_sem);  // Start with semaphore available
#endif

    // Initialize TinyUSB (DWC2 driver handles PHY init internally)
    // Note: When CONFIG_ESP_CONSOLE_USB_CDC is enabled, ESP-IDF ROM already
    // initializes TinyUSB with fixed "Espressif"/"ESP32-S3" descriptors.
    // In that case, we skip init and rely on ROM's USB task.
    bool rom_initialized = tud_inited();

    if (!rom_initialized) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0) && defined(CONFIG_SOC_USB_OTG_SUPPORTED) && CONFIG_SOC_USB_OTG_SUPPORTED
        if (!usb_phy_init_once()) {
            LOG_E("USB", "USB PHY init failed");
            return false;
        }
#endif
        if (!tusb_init()) {
            LOG_E("USB", "TinyUSB init failed");
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(10));

        // Create USB task only if we initialized TinyUSB ourselves
        // Stack size 8192: CCID/OpenPGP needs ~1KB for DO 0x6E + TROPIC01 calls
        xTaskCreate(usb_device_task, "usbd", 8192, nullptr,
                    configMAX_PRIORITIES - 1, &g_usb_task);
    } else {
        // ROM's TinyUSB is active (CONFIG_ESP_CONSOLE_USB_CDC mode)
        // Only CDC serial works, HID interfaces are NOT available
        LOG_I("USB", "Using ROM TinyUSB (console mode)");
#if FEATURE_FIDO2_USB || FEATURE_USB_KEYBOARD
        LOG_W("USB", "HID interfaces NOT available in ROM console mode!");
        LOG_W("USB", "Disable CONFIG_ESP_CONSOLE_USB_CDC for custom USB features");
#endif
    }

    g_usb_initialized = true;

#if FEATURE_GPG_CCID
    // Initialize CCID/OpenPGP after USB is ready
    if (!usb_ccid::init()) {
        LOG_W("USB", "CCID init failed (continuing without CCID)");
    }
#endif

#if CONFIG_ESP_CONSOLE_USB_CDC
    LOG_I("USB", "USB initialized: CDC only (ROM mode)");
#else
    // Log which interfaces are enabled
    LOG_I("USB", "USB initialized: CDC%s%s%s",
          FEATURE_FIDO2_USB ? " + FIDO" : "",
          FEATURE_USB_KEYBOARD ? " + Keyboard" : "",
          FEATURE_GPG_CCID ? " + CCID" : "");
#endif
    return true;
}

bool usb_hid_ready(void) {
    return g_usb_initialized && tud_ready();
}

// ============================================================================
// FIDO HID
// ============================================================================

namespace usb_fido {

bool ready(void) {
#if FEATURE_FIDO2_USB
    return g_usb_initialized && tud_hid_n_ready(USB_HID_INSTANCE_FIDO);
#else
    return false;  // Stub: FIDO2 disabled
#endif
}

bool available(void) {
#if FEATURE_FIDO2_USB
    return g_fido_queue && (uxQueueMessagesWaiting(g_fido_queue) > 0);
#else
    return false;  // Stub: FIDO2 disabled
#endif
}

uint16_t read(uint8_t* buffer) {
    return read_timeout(buffer, 0);
}

uint16_t read_timeout(uint8_t* buffer, uint32_t timeout_ms) {
#if FEATURE_FIDO2_USB
    if (!buffer || !g_fido_queue) return 0;

    fido_packet_t packet;
    TickType_t ticks = (timeout_ms == 0) ? 0 : pdMS_TO_TICKS(timeout_ms);

    if (xQueueReceive(g_fido_queue, &packet, ticks) == pdTRUE) {
        memcpy(buffer, packet.data, 64);
        return 64;
    }
#else
    (void)buffer;
    (void)timeout_ms;
#endif
    return 0;
}

bool write(const uint8_t* buffer) {
#if FEATURE_FIDO2_USB
    if (!buffer || !ready()) return false;

    // Wait for previous TX to complete (with timeout)
    // This ensures the host has received the previous packet
    if (g_fido_tx_sem) {
        if (xSemaphoreTake(g_fido_tx_sem, pdMS_TO_TICKS(100)) != pdTRUE) {
            LOG_W("USB", "FIDO TX semaphore timeout - previous packet not acknowledged");
            // Continue anyway, but may cause issues
        }
    }

    bool result = tud_hid_n_report(USB_HID_INSTANCE_FIDO, 0, buffer, 64);
    if (!result && g_fido_tx_sem) {
        // Report failed, give back the semaphore
        xSemaphoreGive(g_fido_tx_sem);
    }
    return result;
#else
    (void)buffer;
    return false;  // Stub: FIDO2 disabled
#endif
}

} // namespace usb_fido

// ============================================================================
// Keyboard HID
// ============================================================================

namespace usb_keyboard {

bool ready(void) {
#if FEATURE_USB_KEYBOARD
    return g_usb_initialized && tud_hid_n_ready(USB_HID_INSTANCE_KEYBOARD);
#else
    return false;  // Stub: USB Keyboard disabled
#endif
}

bool type(const char* str) {
#if FEATURE_USB_KEYBOARD
    if (!str || !ready()) return false;

    uint8_t keys[6] = {0};
    uint8_t empty[6] = {0};

    // Small delay before first keystroke
    vTaskDelay(pdMS_TO_TICKS(50));

    for (const char* p = str; *p; p++) {
        uint8_t keycode = 0;
        uint8_t modifier = 0;
        char c = *p;

        // US keyboard layout mapping
        if (c >= 'a' && c <= 'z') {
            keycode = HID_KEY_A + (c - 'a');
        } else if (c >= 'A' && c <= 'Z') {
            keycode = HID_KEY_A + (c - 'A');
            modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
        } else if (c >= '1' && c <= '9') {
            keycode = HID_KEY_1 + (c - '1');
        } else if (c == '0') {
            keycode = HID_KEY_0;
        } else if (c == '\n' || c == '\r') {
            keycode = HID_KEY_ENTER;
        } else if (c == ' ') {
            keycode = HID_KEY_SPACE;
        } else if (c == '-') {
            keycode = HID_KEY_MINUS;
        } else if (c == '.') {
            keycode = HID_KEY_PERIOD;
        } else if (c == '/') {
            keycode = HID_KEY_SLASH;
        } else if (c == ':') {
            keycode = HID_KEY_SEMICOLON;
            modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
        } else if (c == '@') {
            keycode = HID_KEY_2;
            modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
        }

        if (keycode) {
            // Key down
            keys[0] = keycode;
            tud_hid_n_keyboard_report(USB_HID_INSTANCE_KEYBOARD,
                                      REPORT_ID_KEYBOARD, modifier, keys);
            vTaskDelay(pdMS_TO_TICKS(20));

            // Key up
            keys[0] = 0;
            tud_hid_n_keyboard_report(USB_HID_INSTANCE_KEYBOARD,
                                      REPORT_ID_KEYBOARD, 0, empty);
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
    return true;
#else
    (void)str;
    return false;  // Stub: USB Keyboard disabled
#endif
}

bool type_enter(void) {
#if FEATURE_USB_KEYBOARD
    if (!ready()) return false;

    uint8_t keys[6] = {HID_KEY_ENTER, 0, 0, 0, 0, 0};
    uint8_t empty[6] = {0};

    tud_hid_n_keyboard_report(USB_HID_INSTANCE_KEYBOARD,
                              REPORT_ID_KEYBOARD, 0, keys);
    vTaskDelay(pdMS_TO_TICKS(20));
    tud_hid_n_keyboard_report(USB_HID_INSTANCE_KEYBOARD,
                              REPORT_ID_KEYBOARD, 0, empty);
    return true;
#else
    return false;  // Stub: USB Keyboard disabled
#endif
}

} // namespace usb_keyboard

// ============================================================================
// CCID SmartCard Interface - Custom TinyUSB Driver
// ============================================================================
// TinyUSB doesn't have native CCID support, so we implement a custom class
// driver that handles the CCID class (0x0B) interface and bulk endpoints.

#if FEATURE_GPG_CCID

// CCID logging - uses error_log_add_direct (USB-safe, viewable via ERRLOG command)
// This writes to both UART and error_log without using console_print
#include "cdc_log.h"
#define CCID_LOG(tag, fmt, ...) error_log_add_direct("[I][%s] " fmt, tag, ##__VA_ARGS__)
#define CCID_LOG_E(tag, fmt, ...) error_log_add_direct("[E][%s] " fmt, tag, ##__VA_ARGS__)
#define CCID_LOG_W(tag, fmt, ...) error_log_add_direct("[W][%s] " fmt, tag, ##__VA_ARGS__)
// To disable CCID logging completely:
// #define CCID_LOG(tag, fmt, ...) do {} while(0)
// #define CCID_LOG_E CCID_LOG
// #define CCID_LOG_W CCID_LOG

// CCID message type names for logging
static const char* ccid_msg_name(uint8_t type) {
    switch (type) {
        case 0x62: return "ICC_POWER_ON";
        case 0x63: return "ICC_POWER_OFF";
        case 0x65: return "GET_SLOT_STATUS";
        case 0x6F: return "XFR_BLOCK";
        case 0x6C: return "GET_PARAMETERS";
        case 0x6D: return "RESET_PARAMETERS";
        case 0x61: return "SET_PARAMETERS";
        case 0x69: return "SECURE";
        case 0x80: return "DATA_BLOCK";
        case 0x81: return "SLOT_STATUS";
        case 0x82: return "PARAMETERS";
        default:   return "UNKNOWN";
    }
}

// Helper to dump hex data
static void ccid_log_hex(const char* prefix, const uint8_t* data, size_t len) {
    if (len == 0) return;
    char hex[128];
    size_t max_bytes = (len > 40) ? 40 : len;
    for (size_t i = 0; i < max_bytes; i++) {
        snprintf(hex + i*3, 4, "%02X ", data[i]);
    }
    if (len > 40) {
        CCID_LOG("CCID", "%s [%zu bytes]: %s...", prefix, len, hex);
    } else {
        CCID_LOG("CCID", "%s [%zu bytes]: %s", prefix, len, hex);
    }
}

// CCID Driver State
static struct {
    bool initialized;
    uint8_t itf_num;
    uint8_t ep_in;
    uint8_t ep_out;
    uint8_t rhport;

    // RX/TX buffers
    uint8_t rx_buf[512];
    uint8_t tx_buf[512];
    uint16_t rx_len;
    bool rx_pending;

    // Stats for debugging
    uint32_t rx_count;
    uint32_t tx_count;
    uint32_t error_count;
} ccid_state;

// Initialize CCID driver (called once at startup)
static void ccid_driver_init(void) {
    CCID_LOG("CCID", "========================================");
    CCID_LOG("CCID", "CCID TinyUSB driver init called");
    CCID_LOG("CCID", "========================================");
    memset(&ccid_state, 0, sizeof(ccid_state));
}

// Reset on bus reset
static void ccid_driver_reset(uint8_t rhport) {
    CCID_LOG("CCID", ">>> RESET on rhport %d", rhport);
    CCID_LOG("CCID", "    Stats before reset: rx=%lu tx=%lu err=%lu",
          ccid_state.rx_count, ccid_state.tx_count, ccid_state.error_count);
    ccid_state.rx_len = 0;
    ccid_state.rx_pending = false;
    ccid_state.initialized = false;
    ccid_state.rx_count = 0;
    ccid_state.tx_count = 0;
    ccid_state.error_count = 0;
}

// Open interface - called when TinyUSB finds matching interface descriptor
static uint16_t ccid_driver_open(uint8_t rhport, tusb_desc_interface_t const *desc_itf, uint16_t max_len) {
    CCID_LOG("CCID", "========================================");
    CCID_LOG("CCID", ">>> OPEN called");
    CCID_LOG("CCID", "    rhport=%d itf=%d class=0x%02X subclass=0x%02X protocol=0x%02X",
          rhport, desc_itf->bInterfaceNumber, desc_itf->bInterfaceClass,
          desc_itf->bInterfaceSubClass, desc_itf->bInterfaceProtocol);
    CCID_LOG("CCID", "    max_len=%d num_endpoints=%d", max_len, desc_itf->bNumEndpoints);

    // Verify this is a CCID interface (class 0x0B)
    if (desc_itf->bInterfaceClass != TUSB_CLASS_SMART_CARD) {
        CCID_LOG("CCID", "    -> Not CCID (class 0x%02X != 0x0B), skipping", desc_itf->bInterfaceClass);
        return 0;  // Not for us
    }

    CCID_LOG("CCID", "    -> CCID interface detected!");

    ccid_state.itf_num = desc_itf->bInterfaceNumber;
    ccid_state.rhport = rhport;

    // Parse descriptors to find endpoints
    uint16_t drv_len = sizeof(tusb_desc_interface_t);
    uint8_t const *p_desc = (uint8_t const *)desc_itf + drv_len;

    CCID_LOG("CCID", "    Parsing descriptors starting at offset %d...", drv_len);

    // Skip CCID functional descriptor (54 bytes, type 0x21)
    while (drv_len < max_len) {
        uint8_t desc_len = p_desc[0];
        uint8_t desc_type = p_desc[1];
        CCID_LOG("CCID", "    Descriptor: len=%d type=0x%02X", desc_len, desc_type);

        if (desc_type == 0x21) {  // CCID functional descriptor
            CCID_LOG("CCID", "      -> CCID Functional Descriptor (%d bytes)", desc_len);
            drv_len += desc_len;
            p_desc += desc_len;
        } else {
            break;
        }
    }

    // Find and open endpoints
    uint8_t ep_count = 0;
    while (drv_len < max_len && ep_count < desc_itf->bNumEndpoints) {
        uint8_t desc_len = p_desc[0];
        uint8_t desc_type = p_desc[1];

        if (desc_type == TUSB_DESC_ENDPOINT) {
            tusb_desc_endpoint_t const *ep_desc = (tusb_desc_endpoint_t const *)p_desc;
            uint8_t ep_addr = ep_desc->bEndpointAddress;
            uint8_t ep_attr = ep_desc->bmAttributes.xfer;
            uint16_t ep_size = ep_desc->wMaxPacketSize;

            CCID_LOG("CCID", "    Endpoint: addr=0x%02X attr=0x%02X size=%d",
                  ep_addr, ep_attr, ep_size);

            if (usbd_edpt_open(rhport, ep_desc)) {
                if (tu_edpt_dir(ep_addr) == TUSB_DIR_IN) {
                    ccid_state.ep_in = ep_addr;
                    CCID_LOG("CCID", "      -> Opened as EP_IN");
                } else {
                    ccid_state.ep_out = ep_addr;
                    CCID_LOG("CCID", "      -> Opened as EP_OUT");
                }
                ep_count++;
            } else {
                CCID_LOG_E("CCID", "      -> FAILED to open endpoint!");
            }
        }
        drv_len += desc_len;
        p_desc += desc_len;
    }

    CCID_LOG("CCID", "    Total endpoints opened: %d", ep_count);

    // Prepare to receive first packet
    if (ccid_state.ep_out) {
        CCID_LOG("CCID", "    Preparing initial RX on EP 0x%02X...", ccid_state.ep_out);
        bool ok = usbd_edpt_xfer(rhport, ccid_state.ep_out, ccid_state.rx_buf, sizeof(ccid_state.rx_buf));
        CCID_LOG("CCID", "    Initial RX prepare: %s", ok ? "OK" : "FAILED");
        ccid_state.rx_pending = ok;
    } else {
        CCID_LOG_E("CCID", "    ERROR: No EP_OUT found!");
    }

    ccid_state.initialized = true;
    CCID_LOG("CCID", ">>> OPEN complete: ep_in=0x%02X ep_out=0x%02X drv_len=%d",
          ccid_state.ep_in, ccid_state.ep_out, drv_len);
    CCID_LOG("CCID", "========================================");

    return drv_len;
}

// Handle control transfers (GET_DESCRIPTOR for CCID, etc.)
static bool ccid_driver_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request) {
    CCID_LOG("CCID", ">>> CONTROL_XFER: stage=%d bmReqType=0x%02X bReq=0x%02X wVal=0x%04X wIdx=0x%04X wLen=%d",
          stage, request->bmRequestType, request->bRequest,
          request->wValue, request->wIndex, request->wLength);

    // Only handle SETUP stage
    if (stage != CONTROL_STAGE_SETUP) {
        CCID_LOG("CCID", "    -> Not SETUP stage, returning true");
        return true;
    }

    // Handle class-specific requests
    if (request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS &&
        request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_INTERFACE &&
        request->wIndex == ccid_state.itf_num) {

        CCID_LOG("CCID", "    -> Class request for our interface");

        switch (request->bRequest) {
            case 0x01:  // CCID_ABORT
                CCID_LOG("CCID", "    -> CCID_ABORT");
                return tud_control_status(rhport, request);

            case 0x02:  // CCID_GET_CLOCK_FREQUENCIES
                CCID_LOG("CCID", "    -> CCID_GET_CLOCK_FREQUENCIES");
                return tud_control_status(rhport, request);

            case 0x03:  // CCID_GET_DATA_RATES
                CCID_LOG("CCID", "    -> CCID_GET_DATA_RATES");
                return tud_control_status(rhport, request);

            default:
                CCID_LOG_W("CCID", "    -> Unknown class request: 0x%02X", request->bRequest);
                return false;
        }
    }

    CCID_LOG("CCID", "    -> Not for us (type=%d rcpt=%d idx=%d vs our itf=%d)",
          request->bmRequestType_bit.type, request->bmRequestType_bit.recipient,
          request->wIndex, ccid_state.itf_num);
    return false;
}

// Handle bulk transfers
static bool ccid_driver_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
    CCID_LOG("CCID", "----------------------------------------");
    CCID_LOG("CCID", ">>> XFER_CB: ep=0x%02X result=%d len=%lu", ep_addr, result, xferred_bytes);

    if (result != XFER_RESULT_SUCCESS) {
        ccid_state.error_count++;
        CCID_LOG_E("CCID", "    Transfer FAILED! result=%d (0=success, 1=failed, 2=stalled)", result);
        return true;
    }

    if (ep_addr == ccid_state.ep_out) {
        // Received data from host
        ccid_state.rx_count++;
        ccid_state.rx_len = xferred_bytes;
        ccid_state.rx_pending = false;

        CCID_LOG("CCID", "=== RX #%lu from host ===", ccid_state.rx_count);

        if (xferred_bytes == 0) {
            CCID_LOG_W("CCID", "    Empty packet received!");
        } else {
            uint8_t msg_type = ccid_state.rx_buf[0];
            CCID_LOG("CCID", "    msg_type=0x%02X (%s)", msg_type, ccid_msg_name(msg_type));

            ccid_log_hex("    RX data", ccid_state.rx_buf, xferred_bytes);
        }

        // Check if we have a complete CCID message (minimum 10 byte header)
        if (ccid_state.rx_len >= 10) {
            uint32_t data_len = ccid_state.rx_buf[1] | (ccid_state.rx_buf[2] << 8) |
                               (ccid_state.rx_buf[3] << 16) | (ccid_state.rx_buf[4] << 24);
            uint8_t slot = ccid_state.rx_buf[5];
            uint8_t seq = ccid_state.rx_buf[6];
            uint32_t total_len = 10 + data_len;

            CCID_LOG("CCID", "    CCID Header: data_len=%lu slot=%d seq=%d", data_len, slot, seq);
            CCID_LOG("CCID", "    Expected total=%lu, received=%d", total_len, ccid_state.rx_len);

            if (ccid_state.rx_len >= total_len) {
                CCID_LOG("CCID", "    Complete message, processing...");

                // Process complete CCID message
                int resp_len = ccid_process_message(ccid_state.rx_buf, total_len,
                                                    ccid_state.tx_buf, sizeof(ccid_state.tx_buf));

                CCID_LOG("CCID", "    ccid_process_message returned: %d", resp_len);

                if (resp_len > 0) {
                    ccid_state.tx_count++;
                    uint8_t resp_type = ccid_state.tx_buf[0];
                    CCID_LOG("CCID", "=== TX #%lu to host ===", ccid_state.tx_count);
                    CCID_LOG("CCID", "    resp_type=0x%02X (%s) len=%d",
                          resp_type, ccid_msg_name(resp_type), resp_len);

                    ccid_log_hex("    TX data", ccid_state.tx_buf, resp_len);

                    // Send response
                    CCID_LOG("CCID", "    Calling usbd_edpt_xfer(rhport=%d, ep=0x%02X, len=%d)...",
                          rhport, ccid_state.ep_in, resp_len);

                    bool ok = usbd_edpt_xfer(rhport, ccid_state.ep_in, ccid_state.tx_buf, resp_len);

                    if (ok) {
                        CCID_LOG("CCID", "    TX queued successfully");
                    } else {
                        ccid_state.error_count++;
                        CCID_LOG_E("CCID", "    TX FAILED to queue!");
                    }
                } else if (resp_len == 0) {
                    CCID_LOG_W("CCID", "    No response generated (resp_len=0)");
                } else {
                    ccid_state.error_count++;
                    CCID_LOG_E("CCID", "    Error processing message (resp_len=%d)", resp_len);
                }
            } else {
                CCID_LOG_W("CCID", "    Incomplete message: need %lu more bytes", total_len - ccid_state.rx_len);
            }
        } else {
            CCID_LOG_W("CCID", "    Message too short for CCID header (need 10, got %d)", ccid_state.rx_len);
        }

        // Prepare for next packet
        CCID_LOG("CCID", "    Preparing next RX...");
        bool rx_ok = usbd_edpt_xfer(rhport, ccid_state.ep_out, ccid_state.rx_buf, sizeof(ccid_state.rx_buf));
        if (rx_ok) {
            CCID_LOG("CCID", "    Next RX prepared OK");
        } else {
            ccid_state.error_count++;
            CCID_LOG_E("CCID", "    Next RX prepare FAILED!");
        }
        ccid_state.rx_pending = rx_ok;
    }
    else if (ep_addr == ccid_state.ep_in) {
        CCID_LOG("CCID", "=== TX complete (ep_in) ===");
        CCID_LOG("CCID", "    %lu bytes sent to host", xferred_bytes);
    }
    else {
        CCID_LOG_W("CCID", "    Unknown endpoint 0x%02X (ep_in=0x%02X, ep_out=0x%02X)",
              ep_addr, ccid_state.ep_in, ccid_state.ep_out);
    }

    CCID_LOG("CCID", "    Stats: rx=%lu tx=%lu err=%lu",
          ccid_state.rx_count, ccid_state.tx_count, ccid_state.error_count);
    CCID_LOG("CCID", "----------------------------------------");

    return true;
}

// CCID driver descriptor - note: name field is always present in TinyUSB structure
static usbd_class_driver_t const ccid_driver = {
    .name             = "CCID",
    .init             = ccid_driver_init,
    .deinit           = NULL,
    .reset            = ccid_driver_reset,
    .open             = ccid_driver_open,
    .control_xfer_cb  = ccid_driver_control_xfer_cb,
    .xfer_cb          = ccid_driver_xfer_cb,
    .xfer_isr         = NULL,
    .sof              = NULL
};

namespace usb_ccid {

bool init(void) {
    CCID_LOG("CCID", "========================================");
    CCID_LOG("CCID", "usb_ccid::init() called");
    // Initialize OpenPGP application (driver init is called by TinyUSB)
    if (!openpgp_init()) {
        LOG_E("CCID", "OpenPGP init failed!");
        return false;
    }
    CCID_LOG("CCID", "CCID SmartCard initialized successfully");
    CCID_LOG("CCID", "========================================");
    return true;
}

bool ready(void) {
    return ccid_state.initialized && g_usb_initialized && tud_ready();
}

void task(void) {
    // All processing happens in xfer_cb, nothing to do here
}

} // namespace usb_ccid

// Register custom CCID driver with TinyUSB
extern "C" {

usbd_class_driver_t const* usbd_app_driver_get_cb(uint8_t *driver_count) {
    CCID_LOG("CCID", "****************************************");
    CCID_LOG("CCID", "usbd_app_driver_get_cb called!");
    CCID_LOG("CCID", "Registering CCID custom driver");
    CCID_LOG("CCID", "****************************************");
    *driver_count = 1;
    return &ccid_driver;
}

} // extern "C"

#endif // FEATURE_GPG_CCID

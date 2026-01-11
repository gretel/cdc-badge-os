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
#include "class/vendor/vendor_device.h"
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
    0xBA, 0x04, 0x01, 0x00,  /* dwFeatures: auto params, auto PPS, auto clock, auto baud, auto IFSD, short+extended APDU */ \
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
        xTaskCreate(usb_device_task, "usbd", 4096, nullptr,
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
    return tud_hid_n_report(USB_HID_INSTANCE_FIDO, 0, buffer, 64);
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
// CCID SmartCard Interface
// ============================================================================

#if FEATURE_GPG_CCID

namespace usb_ccid {

// CCID state
static bool g_ccid_initialized = false;
static uint8_t g_ccid_itf_num = 0;

// RX/TX buffers for CCID messages
// Keep small to save RAM - APDU commands are typically <256 bytes
#define CCID_RX_BUFSIZE 384
#define CCID_TX_BUFSIZE 384
static uint8_t ccid_rx_buf[CCID_RX_BUFSIZE];
static uint8_t ccid_tx_buf[CCID_TX_BUFSIZE];
static uint16_t ccid_rx_len = 0;

bool init(void) {
    if (g_ccid_initialized) return true;

    // Initialize OpenPGP application
    if (!openpgp_init()) {
        LOG_E("CCID", "OpenPGP init failed");
        return false;
    }

    g_ccid_initialized = true;
    LOG_I("CCID", "CCID SmartCard initialized");
    return true;
}

bool ready(void) {
    return g_ccid_initialized && g_usb_initialized && tud_vendor_mounted();
}

// Process CCID message and generate response
static int process_ccid_message(const uint8_t* msg, uint16_t msg_len,
                                 uint8_t* resp, uint16_t resp_max) {
    return ccid_process_message(msg, msg_len, resp, resp_max);
}

// Task function called from USB task to process CCID
void task(void) {
    if (!ready()) return;

    // Check for incoming data
    uint32_t available = tud_vendor_available();
    if (available > 0) {
        uint16_t read_len = tud_vendor_read(ccid_rx_buf + ccid_rx_len,
                                            CCID_RX_BUFSIZE - ccid_rx_len);
        ccid_rx_len += read_len;

        // Check if we have a complete CCID message (minimum 10 byte header)
        if (ccid_rx_len >= 10) {
            // Extract message length from header (bytes 1-4, little endian)
            uint32_t data_len = ccid_rx_buf[1] | (ccid_rx_buf[2] << 8) |
                               (ccid_rx_buf[3] << 16) | (ccid_rx_buf[4] << 24);
            uint32_t total_len = 10 + data_len;

            if (ccid_rx_len >= total_len) {
                // Process complete message
                int resp_len = process_ccid_message(ccid_rx_buf, total_len,
                                                    ccid_tx_buf, CCID_TX_BUFSIZE);

                // Send response
                if (resp_len > 0) {
                    tud_vendor_write(ccid_tx_buf, resp_len);
                    tud_vendor_flush();
                }

                // Remove processed message from buffer
                if (ccid_rx_len > total_len) {
                    memmove(ccid_rx_buf, ccid_rx_buf + total_len,
                            ccid_rx_len - total_len);
                }
                ccid_rx_len -= total_len;
            }
        }
    }
}

} // namespace usb_ccid

// TinyUSB Vendor callbacks for CCID
extern "C" {

// Note: TinyUSB vendor_rx_cb signature includes buffer and size parameters
void tud_vendor_rx_cb(uint8_t itf, uint8_t const* buffer, uint16_t bufsize) {
    (void)itf;
    (void)buffer;
    (void)bufsize;
    // Processing happens in usb_ccid::task() which reads via tud_vendor_read()
}

void tud_vendor_tx_cb(uint8_t itf, uint32_t sent_bytes) {
    (void)itf;
    (void)sent_bytes;
    tud_vendor_write_flush();
}

// Custom CCID driver registration
// TinyUSB allows adding custom class drivers via usbd_app_driver_get_cb
// However, since we're using the standard Vendor class with our own protocol,
// we don't need a custom driver - just the vendor class callbacks above.

} // extern "C"

#endif // FEATURE_GPG_CCID

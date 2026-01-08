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
}

#include <string.h>

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

// Configuration Descriptor
// Calculate total length based on enabled features
#define CONFIG_TOTAL_LEN ( \
    TUD_CONFIG_DESC_LEN + \
    TUD_CDC_DESC_LEN + \
    (FEATURE_FIDO2_USB ? TUD_HID_INOUT_DESC_LEN : 0) + \
    (FEATURE_USB_KEYBOARD ? TUD_HID_DESC_LEN : 0) \
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

#if CONFIG_ESP_CONSOLE_USB_CDC
    LOG_I("USB", "USB initialized: CDC only (ROM mode)");
#else
    // Log which interfaces are enabled
    LOG_I("USB", "USB initialized: CDC%s%s",
          FEATURE_FIDO2_USB ? " + FIDO" : "",
          FEATURE_USB_KEYBOARD ? " + Keyboard" : "");
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

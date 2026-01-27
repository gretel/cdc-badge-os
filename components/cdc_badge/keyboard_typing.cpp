// Keyboard Typing Abstraction Implementation
// Routes typing requests to USB or BLE HID

#include "keyboard_typing.h"

#if FEATURE_USB_KEYBOARD
#include "usb_hid.h"
#endif

#if FEATURE_BLE_HID
#include "ble_hid.h"
#endif

keyboard_transport_t keyboard_get_transport(void) {
#if FEATURE_USB_KEYBOARD
    // Check if USB keyboard is ready
    if (usb_keyboard::ready()) {
        return KEYBOARD_TRANSPORT_USB;
    }
#endif

#if FEATURE_BLE_HID
    // Check if BLE HID is ready
    if (ble_hid_ready()) {
        return KEYBOARD_TRANSPORT_BLE;
    }
#endif

    return KEYBOARD_TRANSPORT_NONE;
}

bool keyboard_is_available(void) {
    return keyboard_get_transport() != KEYBOARD_TRANSPORT_NONE;
}

bool keyboard_type(const char* str, bool press_enter) {
    if (!str) return false;

    keyboard_transport_t transport = keyboard_get_transport();

    switch (transport) {
#if FEATURE_USB_KEYBOARD
        case KEYBOARD_TRANSPORT_USB:
            if (!usb_keyboard::type_string(str)) return false;
            if (press_enter) {
                return usb_keyboard::type_enter();
            }
            return true;
#endif

#if FEATURE_BLE_HID
        case KEYBOARD_TRANSPORT_BLE:
            return ble_hid_type_with_enter(str, press_enter);
#endif

        default:
            return false;
    }
}

bool keyboard_press_enter(void) {
    keyboard_transport_t transport = keyboard_get_transport();

    switch (transport) {
#if FEATURE_USB_KEYBOARD
        case KEYBOARD_TRANSPORT_USB:
            return usb_keyboard::type_enter();
#endif

#if FEATURE_BLE_HID
        case KEYBOARD_TRANSPORT_BLE:
            return ble_hid_press_enter();
#endif

        default:
            return false;
    }
}

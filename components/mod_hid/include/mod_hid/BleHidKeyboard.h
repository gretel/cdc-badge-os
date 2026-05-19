#pragma once

#include "cdc_core/IKeyboardProvider.h"
#include <cstdint>
#include <cstddef>

namespace cdc::mod_hid {

/**
 * Unicode input method for non-ASCII characters
 */
enum class UnicodeMethod : uint8_t {
    ASCII_ONLY = 0,  // Fallback: ae, oe, ue (works everywhere)
    WINDOWS,         // Alt + Numpad codes
    LINUX,           // Ctrl+Shift+U + hex + Enter
    MACOS,           // Option + key sequences (limited)
};

/**
 * BLE HID Keyboard Implementation
 *
 * Implements IKeyboardProvider using BLE HID over GATT (HOGP).
 * Uses IBluetoothController API for GATT registration and notifications.
 */
class BleHidKeyboard : public core::IKeyboardProvider {
public:
    static BleHidKeyboard& instance();

    // Lifecycle
    bool init();
    void deinit();

    // IKeyboardProvider implementation
    bool isConnected() const override;
    bool typeString(const char* text, uint16_t delayMs = 50) override;
    bool typeChar(char c) override;
    bool isBusy() const override;
    void cancel() override;
    const char* getStatusText() const override;

    // Configuration
    void setUnicodeMethod(UnicodeMethod method);
    UnicodeMethod getUnicodeMethod() const { return unicodeMethod_; }

    // BLE HID control
    bool startAdvertising();
    void stopAdvertising();
    bool isAdvertising() const;

    // Connection callbacks (called from BluetoothController)
    void onConnect(uint16_t connHandle);
    void onDisconnect(uint16_t connHandle, int reason);
    void onHostSuspend();
    void onHostResume();

    /**
     * Send a multi-key HID report (up to 6 simultaneous keycodes).
     * \param modifier Modifier bitmask (GattHid Modifier flags).
     * \param keycodes Array of up to 6 keycodes; unused slots must be 0.
     * \param numKeys Number of valid entries in keycodes (1..6).
     * \return true if notification was sent.
     */
    bool sendKeyReport(uint8_t modifier, const uint8_t* keycodes, uint8_t numKeys);

private:
    BleHidKeyboard() = default;

    // Internal typing helpers
    bool sendKeyReport(uint8_t modifier, uint8_t keycode);
    bool releaseAllKeys();
    bool typeAsciiChar(char c);
    bool typeUnicodeChar(uint32_t codepoint);
    bool typeWindowsUnicode(uint32_t codepoint);
    bool typeLinuxUnicode(uint32_t codepoint);
    bool typeMacOsUnicode(uint32_t codepoint);
    bool typeAsciiFallback(uint32_t codepoint);

    // UTF-8 parsing
    static int utf8ToCodepoint(const char* utf8, uint32_t* codepoint);

    // State
    bool initialized_ = false;
    bool advertising_ = false;
    bool busy_ = false;
    bool cancelRequested_ = false;
    UnicodeMethod unicodeMethod_ = UnicodeMethod::ASCII_ONLY;
    char statusText_[48] = "Disconnected";

    // Settings persistence
    void loadSettings();
    void saveSettings();
};

} // namespace cdc::mod_hid

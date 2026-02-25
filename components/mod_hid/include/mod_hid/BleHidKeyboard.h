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
 * Uses NimBLE's built-in HID service when available.
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

    // Connection callbacks (called from NimBLE)
    void onConnect(uint16_t connHandle);
    void onDisconnect(uint16_t connHandle, int reason);
    void onHostSuspend();
    void onHostResume();

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
    uint16_t connHandle_ = 0xFFFF;  // BLE_HS_CONN_HANDLE_NONE
    bool busy_ = false;
    bool cancelRequested_ = false;
    UnicodeMethod unicodeMethod_ = UnicodeMethod::ASCII_ONLY;
    char statusText_[48] = "Disconnected";

    // Settings persistence
    void loadSettings();
    void saveSettings();
};

} // namespace cdc::mod_hid

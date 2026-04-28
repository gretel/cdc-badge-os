---
title: "[LOW] BLE HID keyboard types at maximum speed without rate limiting"
severity: LOW
domain: rate-abuse
lens: rate-abuse-hid
labels:
  - "audit:security/rate-abuse"
---

## Summary
The BLE HID keyboard implementation in `components/mod_hid/src/BleHidKeyboard.cpp` types characters at maximum speed (10ms delay between keypresses) without any rate limiting or configurable typing speed. The `typeAsciiChar()` function uses a fixed 10ms delay, which can be too fast for some receivers and allows rapid-fire typing up to ~50 characters/second.

**Location:** `components/mod_hid/src/BleHidKeyboard.cpp:368-386`

```cpp
bool BleHidKeyboard::typeAsciiChar(char c) {
    KeyMapping mapping = getKeyMapping(c);
    if (mapping.keycode == KeyCode::KEY_NONE) {
        return false;
    }

    if (!sendKeyReport(mapping.modifier, mapping.keycode)) {
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(10));  // Fixed 10ms delay

    if (!releaseAllKeys()) {
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(10));  // Fixed 10ms delay
    return true;
}
```

**Location:** `components/mod_hid/src/BleHidKeyboard.cpp:312-344`

The `typeString()` function accepts a `delayMs` parameter but only applies it between characters, not to the individual keypress/release cycle. When called with `delayMs=0` (or from UI without explicit delay), typing happens at maximum speed.

## Impact
**Typing speed issues:**
- 10ms delay between keypress + release = ~50 characters/second maximum
- Some receivers (slow USB hubs, remote desktop, VMs) may drop keystrokes at this speed
- No way to slow down typing for compatibility without code changes
- Rapid typing can overwhelm the BLE stack or HID buffer

**Denial of service (typing flood):**
- An attacker with BLE access can send rapid-fire keystrokes
- No rate limiting on how many characters can be typed per second
- Can type ~500 characters in 10 seconds (10ms delay each)
- Useful for:
  - Filling form fields with junk data
  - Blocking legitimate typing (keyboard busy)
  - Triggering autocomplete/search at high speed
  - Sending rapid escape sequences to terminals

**No typing speed configuration:**
- `setUnicodeMethod()` exists for Unicode handling
- No `setTypingSpeed()` or similar configuration
- Users cannot adjust typing speed via UI or serial commands
- Hard-coded 10ms delay in `typeAsciiChar()`

## Evidence
- **File:** `components/mod_hid/src/BleHidKeyboard.cpp`
- **Lines 368-386:** `typeAsciiChar()` - fixed 10ms delay
- **Lines 312-344:** `typeString()` - optional delay between characters only
- **Line 314:** `if (busy_) return false;` - single busy flag, no rate limiting
- **Lines 391-479:** Unicode typing methods - also use `typeAsciiChar()` with 10ms delay

**No rate limiting found in:**
- `typeAsciiChar()` function
- `typeUnicodeChar()` function
- `typeString()` function
- `sendKeyReport()` function
- Any middleware or wrapper around typing functions

## Recommended Fix
Add configurable typing speed with sensible defaults and rate limiting:

**Option 1: Add typing speed configuration**
```cpp
// Add to BleHidKeyboard.h (class members)
private:
    uint16_t typingDelayMs_ = 50;  // Default 50ms (20 chars/sec)
    static constexpr uint16_t MIN_TYPING_DELAY_MS = 10;
    static constexpr uint16_t MAX_TYPING_DELAY_MS = 500;

// Add to BleHidKeyboard.h (public method)
public:
    void setTypingSpeed(uint16_t delayMs);
    uint16_t getTypingSpeed() const { return typingDelayMs_; }

// Modify typeAsciiChar() (line 368)
bool BleHidKeyboard::typeAsciiChar(char c) {
    KeyMapping mapping = getKeyMapping(c);
    if (mapping.keycode == KeyCode::KEY_NONE) {
        return false;
    }

    if (!sendKeyReport(mapping.modifier, mapping.keycode)) {
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(typingDelayMs_));  // Configurable delay

    if (!releaseAllKeys()) {
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(typingDelayMs_));  // Configurable delay
    return true;
}

// Implementation
void BleHidKeyboard::setTypingSpeed(uint16_t delayMs) {
    typingDelayMs_ = std::clamp(delayMs, MIN_TYPING_DELAY_MS, MAX_TYPING_DELAY_MS);
}
```

**Option 2: Add global rate limiting**
```cpp
// Add to BleHidKeyboard.cpp (static variables)
static constexpr uint32_t MAX_CHARS_PER_SECOND = 30;
static constexpr uint32_t RATE_LIMIT_MS = 1000 / MAX_CHARS_PER_SECOND;
static uint32_t s_last_keypress_ms = 0;

// Modify typeAsciiChar() (line 368)
bool BleHidKeyboard::typeAsciiChar(char c) {
    uint32_t now = esp_timer_get_time() / 1000;  // ms
    
    // Rate limit: max 30 chars/second
    if (now - s_last_keypress_ms < RATE_LIMIT_MS) {
        vTaskDelay(pdMS_TO_TICKS(RATE_LIMIT_MS - (now - s_last_keypress_ms)));
    }
    s_last_keypress_ms = esp_timer_get_time() / 1000;
    
    // ... rest of function
}
```

**Option 3: Add serial command for typing speed**
```cpp
// Add to HID module (GpgModule.cpp pattern)
static void cmd_hid_speed(const char* args) {
    if (!args || !*args) {
        cdc::serial::Console::printf("Current typing speed: %ums\r\n", 
                                     BleHidKeyboard::instance().getTypingSpeed());
        return;
    }
    uint16_t speed = static_cast<uint16_t>(atoi(args));
    BleHidKeyboard::instance().setTypingSpeed(speed);
    cdc::serial::Console::printf("Typing speed set to %ums\r\n", speed);
}

// Register command
registry.registerCommand({"HID_SPEED", "Set typing speed (ms)", cmd_hid_speed, "hid", false});
```

**Recommended implementation (Option 1 + 3):**
1. Add `typingDelayMs_` member to `BleHidKeyboard` class (default 50ms)
2. Add `setTypingSpeed()` and `getTypingSpeed()` methods
3. Modify `typeAsciiChar()` to use configurable delay
4. Add serial command `HID_SPEED` to adjust typing speed
5. Store typing speed in NVS for persistence
6. Add UI menu item in HID module to adjust typing speed

## References
- BLE HID spec: [Keyboard Input Report](https://www.bluetooth.com/specifications/specs/hid-service-1-0/)
- USB HID spec: [Keyboard boot protocol](https://usb.org/document-library/device-class-definition-hid-111)
- OWASP: [Rate Limiting Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Rate_Limiting_Cheat_Sheet.html)

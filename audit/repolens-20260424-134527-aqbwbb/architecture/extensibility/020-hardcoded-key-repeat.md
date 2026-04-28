---
title: "[LOW] Hardcoded Key Repeat Delays"
severity: LOW
domain: architecture/extensibility
lens: input-handling
labels:
  - "audit:architecture/extensibility"
---

## Summary
Key repeat delays and rates are hardcoded in the keypad handling logic. Different users or contexts (e.g., PIN entry vs. text input) may require different repeat behavior.

**Files affected:**
- `components/cdc_hal/src/TCA9535Keypad.cpp`
- `components/cdc_views/src/PinEntryView.cpp`
- `components/cdc_views/src/T9InputView.cpp`

## Impact
- **User customization**: Cannot adjust key repeat for user preferences
- **Context-aware behavior**: PIN entry might want slower repeat, text input faster
- **Accessibility**: Users with different motor skills may need different repeat rates

## Evidence

### Hardcoded Repeat Logic
```cpp
// components/cdc_hal/src/TCA9535Keypad.cpp (typical pattern)
static constexpr uint32_t KEY_REPEAT_DELAY_MS = 500;
static constexpr uint32_t KEY_REPEAT_RATE_MS = 50;

// Used in keypad scanning loop
if (keyPressedTime_ > KEY_REPEAT_DELAY_MS) {
    repeatCounter_++;
    if (repeatCounter_ % (KEY_REPEAT_RATE_MS / tickMs) == 0) {
        // Fire repeat event
    }
}
```

## Recommended Fix

### Create Input Profile System
```cpp
// components/cdc_hal/include/cdc_hal/InputProfile.h
#pragma once
#include <cstdint>

namespace cdc::hal {

struct InputProfile {
    uint32_t repeatDelayMs;    // Initial delay before repeat
    uint32_t repeatRateMs;     // Time between repeats
    uint32_t longPressMs;      // Time for long press detection
};

class InputProfileManager {
public:
    static InputProfileManager& instance();
    
    // Get current profile
    const InputProfile& getProfile() const;
    
    // Set profile by name
    void setProfile(const char* name);
    
    // Set profile directly
    void setProfile(const InputProfile& profile);
    
    // Built-in profiles
    static const InputProfile& profileDefault();
    static const InputProfile& profilePinEntry();  // Slower repeat
    static const InputProfile& profileFast();      // Faster repeat
};

} // namespace cdc::hal
```

### Refactor Keypad to Use Profiles
```cpp
// components/cdc_hal/src/TCA9535Keypad.cpp
class TCA9535Keypad {
    InputProfile profile_;
    
public:
    void setInputProfile(const InputProfile& profile) {
        profile_ = profile;
    }
    
    void update(uint32_t nowMs) {
        if (keyPressedTime_ > profile_.repeatDelayMs) {
            if ((nowMs - lastRepeatTime_) >= profile_.repeatRateMs) {
                // Fire repeat event
                lastRepeatTime_ = nowMs;
            }
        }
    }
};
```

### Context-Aware Profiles in Views
```cpp
// components/cdc_views/src/PinEntryView.cpp
void PinEntryView::onEnter(void* context) {
    // Use slower repeat for PIN entry
    auto& manager = hal::InputProfileManager::instance();
    manager.setProfile(hal::InputProfileManager::profilePinEntry());
}
```

## References
- Input Profile Pattern: https://gamedev.net/forums/topic/580400-input-profile-system/
- Accessibility guidelines: https://www.w3.org/WAI/WCAG21/Understanding/timing-adjustable.html

</content>
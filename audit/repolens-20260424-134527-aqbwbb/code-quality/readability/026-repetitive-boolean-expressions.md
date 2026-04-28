---
title: "[026] [MEDIUM] Repetitive inline boolean expressions in switch statements"
severity: MEDIUM
domain: code-quality/readability
lens: unclear-control-flow
labels:
  - "audit:code-quality/readability"
---

## Summary

Multiple switch statements contain repetitive inline boolean expressions that check similar conditions across different cases. These expressions are repeated 3-5 times each, making it difficult to understand the decision logic without mentally evaluating each condition.

**Affected files:**
- `components/mod_gpg/src/openpgp/openpgp.cpp:1145-1198` - PIN change validation with repeated loop patterns
- `components/cdc_os_ui/src/AppUi.cpp:414-440` - Menu selection switch with repeated null checks
- `components/cdc_hal/src/TCA9535Keypad.cpp:48-79` - Key-to-mask conversion with repetitive binary literals

## Impact

**Readability:** Developers must parse the same logical pattern multiple times instead of recognizing a single abstraction.

**Maintenance:** If the validation logic changes, all duplicate locations must be updated, increasing the chance of inconsistencies.

**Bug risk:** Subtle variations in repeated code (e.g., different loop bounds or condition order) can introduce hard-to-spot bugs.

## Evidence

### openpgp.cpp - Duplicate PIN validation loops in change_reference_data

Lines 1145-1198 contain nearly identical loops for PW1 and PW3 validation:

```cpp
// PW1 change (lines 1145-1167)
bool changed = false;
for (size_t old_len = OPENPGP_PW1_MIN_LEN; old_len <= apdu->lc - OPENPGP_PW1_MIN_LEN; old_len++) {
    char old_pin[OPENPGP_PIN_MAX_LEN + 1];
    char new_pin[OPENPGP_PIN_MAX_LEN + 1];
    
    memcpy(old_pin, apdu->data, old_len);
    old_pin[old_len] = '\0';
    
    size_t new_len = apdu->lc - old_len;
    memcpy(new_pin, apdu->data + old_len, new_len);
    new_pin[new_len] = '\0';
    
    if (pin_storage_openpgp_verify_pw1(old_pin)) {
        if (pin_storage_openpgp_change_pw1(new_pin)) {
            changed = true;
            break;
        }
    }
}

// PW3 change (lines 1170-1192) - IDENTICAL PATTERN
bool changed = false;
for (size_t old_len = OPENPGP_PW3_MIN_LEN; old_len <= apdu->lc - OPENPGP_PW3_MIN_LEN; old_len++) {
    char old_pin[OPENPGP_PIN_MAX_LEN + 1];
    char new_pin[OPENPGP_PIN_MAX_LEN + 1];
    
    memcpy(old_pin, apdu->data, old_len);
    old_pin[old_len] = '\0';
    
    size_t new_len = apdu->lc - old_len;
    memcpy(new_pin, apdu->data + old_len, new_len);
    new_pin[new_len] = '\0';
    
    if (pin_storage_openpgp_verify_pw3(old_pin)) {
        if (pin_storage_openpgp_change_pw3(new_pin)) {
            changed = true;
            break;
        }
    }
}
```

**Problems:**
- 25+ lines of nearly identical code, differing only in PIN type (PW1 vs PW3) and minimum length constants
- The loop structure, variable declarations, and control flow are duplicated verbatim
- A developer reading this must parse the same logic twice to understand the pattern

### AppUi.cpp - Repetitive null checks in menu switches

Lines 414-440:

```cpp
static void onMainMenuSelect(uint16_t index, void* userData) {
    (void)userData;
    
    if (index < s_mainMenuPluginCount) {
        auto& item = s_mainMenuModuleItems[index];
        if (item.getView) {
            IView* view = item.getView();
            if (view) ViewStack::instance().push(view);  // Line 419
        }
        return;
    }
    // ... similar pattern at line 427-430
}

static void onToolsSelect(uint16_t index, void* userData) {
    (void)userData;
    
    switch (index) {
        case 0: showModulesView(); return;
        case 1: showWifiMainMenu(); return;
        case 2: showBluetoothMenu(); return;
        case 3: showExpertMenu(); return;
    }
    
    uint8_t moduleIdx = index - TOOLS_FIXED_COUNT;
    if (moduleIdx < s_toolsModuleCount) {
        auto& item = s_toolsModuleItems[moduleIdx];
        if (item.getView) {
            IView* view = item.getView();
            if (view) ViewStack::instance().push(view);  // Line 438-440
        }
    }
}
```

**Problems:**
- Same `if (item.getView) { IView* view = item.getView(); if (view) ... }` pattern appears 3 times in the file
- Each occurrence is 3 lines of nearly identical code
- Could be extracted into a single helper function

### TCA9535Keypad.cpp - Repetitive binary literals for key mapping

Lines 48-79:

```cpp
static Key rawToKey(uint16_t raw) {
    switch (raw & 0x0FFF) {
        case 0b111111111110: return Key::KEY_0;
        case 0b111111111101: return Key::KEY_1;
        case 0b111111111011: return Key::KEY_2;
        case 0b111111110111: return Key::KEY_3;
        case 0b111111101111: return Key::KEY_4;
        case 0b111111011111: return Key::KEY_5;
        case 0b111110111111: return Key::KEY_6;
        case 0b111101111111: return Key::KEY_7;
        case 0b111011111111: return Key::KEY_8;
        case 0b110111111111: return Key::KEY_9;
        case 0b011111111111: return Key::KEY_NO;
        case 0b101111111111: return Key::KEY_YES;
        default: return Key::KEY_NONE;
    }
}

static uint16_t keyToMask(Key key) {
    switch (key) {
        case Key::KEY_0: return 0b111111111110;
        case Key::KEY_1: return 0b111111111101;
        case Key::KEY_2: return 0b111111111011;
        // ... 12 cases total, all binary literals
    }
}
```

**Problems:**
- Two functions with inverse logic, both using the same 12 binary literals
- Hard to verify correctness without counting bits
- No explanation of why these specific bit patterns (active-low 12-key matrix)
- Could use a single lookup table instead of 24 switch cases

## Recommended Fix

### 1. Extract PIN validation into a generic helper function

**Before:**
```cpp
// Duplicate loops for PW1 and PW3
for (size_t old_len = OPENPGP_PW1_MIN_LEN; ...) { ... }
for (size_t old_len = OPENPGP_PW3_MIN_LEN; ...) { ... }
```

**After:**
```cpp
static bool change_pin_generic(
    const char* old_pin, 
    const char* new_pin,
    size_t min_len,
    std::function<bool(const char*)> verify_fn,
    std::function<bool(const char*)> change_fn
) {
    for (size_t len = min_len; len <= strlen(old_pin); len++) {
        if (verify_fn(old_pin)) {
            if (change_fn(new_pin)) {
                return true;
            }
        }
    }
    return false;
}

// Usage:
if (change_pin_generic(old_pin, new_pin, OPENPGP_PW1_MIN_LEN, 
                       pin_storage_openpgp_verify_pw1,
                       pin_storage_openpgp_change_pw1)) {
    // ...
}
```

### 2. Create a helper for view navigation

**Before:**
```cpp
if (item.getView) {
    IView* view = item.getView();
    if (view) ViewStack::instance().push(view);
}
```

**After:**
```cpp
static void push_view_if_available(const core::ModuleMenuItem& item) {
    if (item.getView) {
        IView* view = item.getView();
        if (view) ViewStack::instance().push(view);
    }
}

// Usage:
push_view_if_available(s_mainMenuModuleItems[index]);
```

### 3. Replace key mapping with lookup tables

**Before:**
```cpp
switch (raw & 0x0FFF) {
    case 0b111111111110: return Key::KEY_0;
    case 0b111111111101: return Key::KEY_1;
    // ... 12 cases
}
```

**After:**
```cpp
/** \brief Key matrix bit patterns (active-low, 12 keys). */
static constexpr struct {
    uint16_t mask;
    Key key;
} KEY_MATRIX[] = {
    {0b111111111110, Key::KEY_0},  // Bit 0 pressed
    {0b111111111101, Key::KEY_1},  // Bit 1 pressed
    {0b111111111011, Key::KEY_2},  // Bit 2 pressed
    // ...
};

static Key rawToKey(uint16_t raw) {
    uint16_t masked = raw & 0x0FFF;
    for (const auto& entry : KEY_MATRIX) {
        if (masked == entry.mask) return entry.key;
    }
    return Key::KEY_NONE;
}
```

### 4. Add explanatory comments for bit patterns

```cpp
/**
 * \brief 12-key keypad bit patterns (active-low).
 * 
 * Layout: TCA9553 provides 16 GPIO pins, we use 12 for keys.
 * Each key pulls one bit low when pressed.
 * 
 * Bit mapping:
 *   Bits 0-9: Keys 0-9 (decimal digits)
 *   Bit 10:   Key NO (Cancel)
 *   Bit 11:   Key YES (OK)
 */
```

## References
- [Clean Code - Chapter 3: Functions](https://cleancodestudent.com/clean-code-chapter-3/) - Extract duplicate logic into functions
- [DRY Principle](https://en.wikipedia.org/wiki/Don%27t_repeat_yourself) - Don't Repeat Yourself
- [C++ Core Guidelines - F.20: Don't repeat yourself](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#f20-dont-repeat-yourself-dry)

</content>
---
title: "[016] [LOW] Nested ternary chains in TotpModule::onWizardDigits"
severity: LOW
domain: code-readability
lens: code-quality/readability
labels:
  - "audit:code-quality/readability"
---

## Summary
In `components/mod_totp/src/TotpModule.cpp:818-828`, the `onWizardDigits` function uses a ternary expression with modulo:

```cpp
static void onWizardDigits(uint16_t index, void* userData) {
    (void)userData;
    static const uint8_t digitMap[3] = {6, 7, 8};
    s_wizard.digits = digitMap[index % 3];

    static ui::ListItem algoItems[3] = {
        {"SHA1", 0, false, nullptr},
        {"SHA256", 0, false, nullptr},
        {"SHA512", 0, false, nullptr}
    };
    if (!s_viewsInitialized) {
        s_algoMenu.setOnSelect(onWizardAlgo);
    }
    s_algoMenu.init(mstr(STR_ALGORITHM), algoItems, 3);
    ui::ViewStack::instance().push(&s_algoMenu);
}
```

Similar patterns appear in `onWizardAlgo` and `onWizardPeriod`:
```cpp
s_wizard.algorithm = static_cast<uint8_t>(index % 3);  // Line 843
s_wizard.period = (index == 0) ? 30 : 60;              // Line 858
```

## Impact
- **Magic numbers**: `index % 3` is unclear - why modulo 3?
- **Implicit assumptions**: The code assumes `index` will always be valid (0-2) but doesn't validate

## Evidence
**File**: `components/mod_totp/src/TotpModule.cpp:818-860`

Three similar functions use index-to-value mapping:
- `onWizardDigits`: `digitMap[index % 3]`
- `onWizardAlgo`: `static_cast<uint8_t>(index % 3)`
- `onWizardPeriod`: `(index == 0) ? 30 : 60`

## Recommended Fix
Add validation and use clearer mapping:

```cpp
static void onWizardDigits(uint16_t index, void* userData) {
    (void)userData;
    static const uint8_t digitMap[] = {6, 7, 8};
    
    if (index >= sizeof(digitMap)) {
        ui::showToastError(mstr(STR_INVALID_INPUT));
        return;
    }
    s_wizard.digits = digitMap[index];

    // ... rest of function
}

static void onWizardAlgo(uint16_t index, void* userData) {
    (void)userData;
    static const uint8_t algoMap[] = {
        static_cast<uint8_t>(TotpAlgorithm::SHA1),
        static_cast<uint8_t>(TotpAlgorithm::SHA256),
        static_cast<uint8_t>(TotpAlgorithm::SHA512)
    };

    if (index >= sizeof(algoMap)) {
        ui::showToastError(mstr(STR_INVALID_INPUT));
        return;
    }
    s_wizard.algorithm = algoMap[index];

    // ... rest of function
}

static void onWizardPeriod(uint16_t index, void* userData) {
    (void)userData;
    static const uint32_t periodMap[] = {30, 60};

    if (index >= sizeof(periodMap)) {
        ui::showToastError(mstr(STR_INVALID_INPUT));
        return;
    }
    s_wizard.period = periodMap[index];

    wizardFinish();
}
```

## References
- [C++ Core Guidelines - ES.46: Avoid lossy conversions](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#es46-avoid-lossy-conversions)
- [C++ Core Guidelines - ES.47: Use type-safe conversion functions](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#es47-use-type-safe-conversion-functions)

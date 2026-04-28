---
title: "[MEDIUM] Complex switch statement in onSettingsSelect() function"
severity: MEDIUM
domain: Code Quality
lens: cyclomatic-complexity
labels:
  - "audit:code-quality/complexity"
---

## Summary

The `onSettingsSelect()` function in `components/cdc_os_ui/src/AppUi.cpp` (lines 437-473) contains a switch statement with 8 cases, each with different logic paths. While not extremely complex, it mixes view creation with navigation logic and could benefit from extraction.

**Estimated Cyclomatic Complexity: ~10** (at threshold)

## Impact

**Maintenance Burden:**
- Adding new settings requires modifying the switch statement
- View creation logic is duplicated across settings handlers
- Hard to test individual settings flows in isolation

**Readability:**
- Function grows as new settings are added
- Some cases have simple push, others have initialization logic

## Evidence

**File:** `components/cdc_os_ui/src/AppUi.cpp:437-473`

**Code excerpt:**
```cpp
static void onSettingsSelect(uint16_t index, void* userData) {
    (void)userData;

    switch (index) {
        case SETTINGS_IDX_BRIGHTNESS:
            ViewStack::instance().push(s_brightnessSlider);
            break;
        case SETTINGS_IDX_LANGUAGE:
            ViewStack::instance().push(s_languageMenu);
            break;
        case SETTINGS_IDX_TIMEZONE:
            ViewStack::instance().push(s_timezoneSlider);
            break;
        case SETTINGS_IDX_AUTO_SLEEP:
            ViewStack::instance().push(s_sleepSlider);
            break;
        case SETTINGS_IDX_BADGE_TEXT:
            settings::startBadgeTextEdit();
            break;
        case SETTINGS_IDX_SET_DATE:
            ViewStack::instance().push(s_dateInput);
            break;
        case SETTINGS_IDX_SET_TIME:
            ViewStack::instance().push(s_timeInput);
            break;
        case SETTINGS_IDX_CHANGE_PIN:
            if (s_pinChangeView) {
                s_pinChangeView->init(core::PinManager::BADGE_PIN_MIN, core::PinManager::BADGE_PIN_MAX);
                ViewStack::instance().push(s_pinChangeView);
            }
            break;
    }
}
```

**Branching count:**
- `switch (index)` with 8 cases (+7)
- Line 468: `if (s_pinChangeView)` (+1)

Total: ~8-10 independent paths

## Recommended Fix

**Use a lookup table for settings handlers:**

1. Define handler function type:
```cpp
using SettingsHandler = std::function<void()>;
```

2. Create handler map:
```cpp
static SettingsHandler getSettingsHandler(uint16_t index) {
    switch (index) {
        case SETTINGS_IDX_BRIGHTNESS:
            return []() { ViewStack::instance().push(s_brightnessSlider); };
        case SETTINGS_IDX_LANGUAGE:
            return []() { ViewStack::instance().push(s_languageMenu); };
        case SETTINGS_IDX_TIMEZONE:
            return []() { ViewStack::instance().push(s_timezoneSlider); };
        case SETTINGS_IDX_AUTO_SLEEP:
            return []() { ViewStack::instance().push(s_sleepSlider); };
        case SETTINGS_IDX_BADGE_TEXT:
            return []() { settings::startBadgeTextEdit(); };
        case SETTINGS_IDX_SET_DATE:
            return []() { ViewStack::instance().push(s_dateInput); };
        case SETTINGS_IDX_SET_TIME:
            return []() { ViewStack::instance().push(s_timeInput); };
        case SETTINGS_IDX_CHANGE_PIN:
            return []() {
                if (s_pinChangeView) {
                    s_pinChangeView->init(core::PinManager::BADGE_PIN_MIN, core::PinManager::BADGE_PIN_MAX);
                    ViewStack::instance().push(s_pinChangeView);
                }
            };
        default:
            return []() {};  // No-op
    }
}
```

3. Simplify main function:
```cpp
static void onSettingsSelect(uint16_t index, void* userData) {
    (void)userData;
    auto handler = getSettingsHandler(index);
    handler();
}
```

**Alternative (array-based):**
```cpp
static const SettingsHandler s_settingsHandlers[] = {
    []() { ViewStack::instance().push(s_brightnessSlider); },
    []() { ViewStack::instance().push(s_languageMenu); },
    []() { ViewStack::instance().push(s_timezoneSlider); },
    []() { ViewStack::instance().push(s_sleepSlider); },
    []() { settings::startBadgeTextEdit(); },
    []() { ViewStack::instance().push(s_dateInput); },
    []() { ViewStack::instance().push(s_timeInput); },
    []() {
        if (s_pinChangeView) {
            s_pinChangeView->init(core::PinManager::BADGE_PIN_MIN, core::PinManager::BADGE_PIN_MAX);
            ViewStack::instance().push(s_pinChangeView);
        }
    }
};

static void onSettingsSelect(uint16_t index, void* userData) {
    (void)userData;
    if (index < sizeof(s_settingsHandlers)/sizeof(s_settingsHandlers[0])) {
        s_settingsHandlers[index]();
    }
}
```

**Expected result:**
- `onSettingsSelect()` reduced to 5 lines
- Each handler is a self-contained unit
- Easier to add settings (just add to array/switch)
- Better testability with isolated handlers

## References

- [Replace Conditional with Polymorphism](https://refactoring.com/catalog/replaceConditionalsWithPolymorphism.html)
- [Strategy Pattern](https://refactoring.com/catalog/introduceStrategy.html)

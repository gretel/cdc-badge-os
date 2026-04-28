---
title: "[LOW] PIN change wizard lacks step descriptions for first-time users"
severity: LOW
domain: information-architecture
lens: help-context
labels:
  - "pin-change"
  - "wizard-flow"
  - "progressive-disclosure"
---

## Summary
The PIN change wizard (components/cdc_os_ui/src/views/PinChangeView.cpp) shows a 3-step process but doesn't explain what each step requires, making it unclear for first-time users what "Current PIN", "New PIN", and "Confirm PIN" mean.

**Evidence:**
- File: `components/cdc_os_ui/src/views/PinChangeView.cpp:36-49`
- Step enum at line 24-27:
```cpp
enum class Step {
    CURRENT_PIN,
    NEW_PIN,
    CONFIRM_PIN
};
```

- Step title at line 90-97:
```cpp
const char* PinChangeView::getStepTitle() const {
    switch (step_) {
        case Step::CURRENT_PIN: return tr(StringId::CURRENT_PIN);
        case Step::NEW_PIN: return tr(StringId::NEW_PIN);
        case Step::CONFIRM_PIN: return tr(StringId::CONFIRM_PIN);
    }
    return "";
}
```

- Rendering shows "1/3: Current PIN" but no explanation of what to enter

The wizard flow is:
1. Enter current PIN (verify existing)
2. Enter new PIN
3. Confirm new PIN (must match step 2)

## Impact
First-time users may:
1. Not understand they need to enter their *current* PIN first (not a new one)
2. Be confused by the "Confirm PIN" step - why enter the new PIN twice?
3. Not realize step 2 and 3 must match exactly
4. Get stuck if they skip the confirmation step mentally

## Evidence
- PIN change view init: `components/cdc_os_ui/src/views/PinChangeView.cpp:36-49`
- Step rendering at line 330-340:
```cpp
const char* stepTitle = getStepTitle();
char stepStr[48];
snprintf(stepStr, sizeof(stepStr), "%d/3: %s", static_cast<int>(step_) + 1, stepTitle);
```

- I18n strings at `components/cdc_ui/src/I18n.cpp:66-69`
- Only labels, no descriptions

## Recommended Fix
Add a brief description under the step title:

**Option 1: Add description to render**
```cpp
void PinChangeView::render(bool partial) {
    // ... existing title code ...

    const char* stepTitle = getStepTitle();
    char stepStr[48];
    snprintf(stepStr, sizeof(stepStr), "%d/3: %s", static_cast<int>(step_) + 1, stepTitle);
    gfx->setCursor((width - w) / 2, STEP_Y);
    gfx->print(stepStr);

    // Add step description
    const char* desc = getStepDescription();  // New method
    gfx->setTextSize(1);
    gfx->setCursor((width - 60) / 2, STEP_Y + 12);
    gfx->print(desc);

    // ... rest of render ...
}

const char* PinChangeView::getStepDescription() const {
    switch (step_) {
        case Step::CURRENT_PIN: return "Enter your current PIN to verify";
        case Step::NEW_PIN: return "Enter your new PIN (4-8 digits)";
        case Step::CONFIRM_PIN: return "Enter the same new PIN again";
    }
    return "";
}
```

**Option 2: Show info view before wizard starts**
```cpp
static void showPinChangeInfo() {
    const char* info = "PIN Change Wizard\n\n"
        "3 Steps:\n"
        "1. Enter current PIN\n"
        "2. Enter new PIN\n"
        "3. Confirm new PIN\n\n"
        "PIN must be 4-8 digits.";
    showInfo("Change PIN", info);
}
```

**Option 3: Add to I18n with longer labels**
```cpp
// In I18n.cpp
REG(CURRENT_PIN, "Current PIN (verify)", "Aktueller PIN (best\u00e4tigen)");
REG(NEW_PIN,     "New PIN",              "Neuer PIN");
REG(CONFIRM_PIN, "Confirm new PIN",      "Neuer PIN wiederholen");
```

## References
- PIN change view: `components/cdc_os_ui/src/views/PinChangeView.cpp`
- I18n strings: `components/cdc_ui/include/cdc_ui/I18n.h:66-69`
- Similar pattern: PinEntryView at `components/cdc_views/src/PinEntryView.cpp:38-47`

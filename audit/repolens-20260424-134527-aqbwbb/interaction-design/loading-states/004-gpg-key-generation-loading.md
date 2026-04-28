---
title: "[LOW] No loading state during GPG key generation"
severity: LOW
domain: interaction-design/loading-states
lens: loading-states
labels:
  - "audit:interaction-design/loading-states"
---

## Summary
When generating GPG keys through the wizard, the operation happens synchronously without any loading indicator. The `onWizardCurve()` function in `components/mod_gpg/src/GpgModule.cpp:452-495` calls `gpg_generate_key()` which performs cryptographic key generation - a potentially slow operation.

**Location**: `components/mod_gpg/src/GpgModule.cpp:452-495`

## Impact
- Key generation can take 1-3 seconds depending on the curve and hardware
- User sees no feedback that the operation is in progress
- User might think the button press was ignored and try again
- E-Paper display refresh adds additional perceived delay

## Evidence
```cpp
// components/mod_gpg/src/GpgModule.cpp:452-495
static void onWizardCurve(uint16_t index, void*) {
    s_wizard.curve = (index == 0) ? CDC_CURVE_ED25519 : CDC_CURVE_P256;
    char user_id[GPG_USER_ID_MAX] = {};
    // ... construct user_id from name and email

    gpg_set_pending_user_id(user_id);
    if (gpg_generate_key(s_wizard.curve)) {  // Synchronous key generation
        ui::showToastSuccess(ui::tr(ui::StringId::OK));
    } else {
        ui::showToastError(ui::tr(ui::StringId::FAILED));
    }
    while (ui::ViewStack::instance().depth() > 1) {
        ui::ViewStack::instance().pop();
    }
}
```

## Recommended Fix
Add a task toast before key generation starts:

```cpp
static void onWizardCurve(uint16_t index, void*) {
    s_wizard.curve = (index == 0) ? CDC_CURVE_ED25519 : CDC_CURVE_P256;
    char user_id[GPG_USER_ID_MAX] = {};
    // ... construct user_id from name and email

    gpg_set_pending_user_id(user_id);

    ui::showToastTask("Generating key...");  // Show loading
    if (gpg_generate_key(s_wizard.curve)) {
        ui::showToastSuccess(ui::tr(ui::StringId::OK));
    } else {
        ui::showToastError(ui::tr(ui::StringId::FAILED));
    }
    while (ui::ViewStack::instance().depth() > 1) {
        ui::ViewStack::instance().pop();
    }
}
```

## References
- Cryptographic operations on ESP32-S3 typically take 1-3 seconds for ECC key generation
- ToastView already supports task indicator: `showToastTask()` in `components/cdc_views/src/ToastView.cpp`

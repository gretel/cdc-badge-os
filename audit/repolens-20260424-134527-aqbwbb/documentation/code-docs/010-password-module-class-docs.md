---
title: "[MEDIUM] Missing module class documentation for PasswordModule"
severity: MEDIUM
domain: documentation/code-docs
lens: code-docs
labels:
  - "audit:documentation/code-docs"
---

## Summary

The `PasswordModule` class in `components/mod_password/include/mod_password/PasswordModule.h` has minimal documentation - only the base class methods are inherited, with no class-level documentation explaining what the module does.

**File:** `components/mod_password/include/mod_password/PasswordModule.h`

## Impact

- New developers don't understand the module's purpose at a glance
- Unclear what features the password module provides
- Makes it harder to maintain/extend the module

## Evidence

```cpp
// File: components/mod_password/include/mod_password/PasswordModule.h
// Line 1-30: No class documentation

#pragma once

#include "cdc_core/IModule.h"

namespace cdc::mod_password {

class PasswordModule : public core::IModule {
public:
    const char* getName() const override { return "mod_password"; }
    core::ServiceState getState() const override { return state_; }
    bool init() override;
    bool start() override;
    void stop() override;

    const char* getVersion() const override { return "1.0"; }
    uint8_t getMenuItems(core::ModuleMenuItem* items, uint8_t maxItems) override;
    core::IModule::SlotRequest getSlotRequest() const override;
    void setSlotRange(const core::IModule::SlotRange& range) override;

    static PasswordModule& instance();

private:
    PasswordModule() = default;
    core::ServiceState state_ = core::ServiceState::UNINITIALIZED;
    core::IModule::SlotRange slotRange_ = {};
};

} // namespace cdc::mod_password

extern "C" void mod_password_register();
```

Compared to well-documented classes like `ListView`:

```cpp
// components/cdc_views/include/cdc_views/ListView.h
/**
 * ListView - Highly dynamic scrollable selection menu
 *
 * Reusable component for any list-based UI.
 * Dynamically calculates visible items based on display size.
 *
 * Keys:
 *   2 = Up
 *   8 = Down
 *   Y = Select (triggers callback)
 *   N = Back (REQUEST_POP)
 */
class ListView : public ViewBase {
```

## Recommended Fix

Add class documentation:

```cpp
#pragma once

#include "cdc_core/IModule.h"

namespace cdc::mod_password {

/**
 * \brief Password Vault Module.
 *
 * Manages a collection of password entries with:
 * - Secure storage in TROPIC01 R-Memory slots
 * - QR code export for mobile devices
 * - BLE HID typing for desktop login
 * - USB HID typing when connected
 *
 * Features:
 * - Add/edit/delete password entries
 * - Categorize by service/domain
 * - Generate strong passwords
 * - Export to QR code for TOTP apps
 *
 * Keys (in password list view):
 *   Y = View details / Edit
 *   N = Delete
 *   3 = Generate QR code
 *   4/6 = Scroll list
 */
class PasswordModule : public core::IModule {
public:
    const char* getName() const override { return "mod_password"; }
    core::ServiceState getState() const override { return state_; }
    bool init() override;
    bool start() override;
    void stop() override;

    const char* getVersion() const override { return "1.0"; }
    uint8_t getMenuItems(core::ModuleMenuItem* items, uint8_t maxItems) override;
    core::IModule::SlotRequest getSlotRequest() const override;
    void setSlotRange(const core::IModule::SlotRange& range) override;

    /**
     * \brief Get singleton instance.
     */
    static PasswordModule& instance();

private:
    PasswordModule() = default;
    core::ServiceState state_ = core::ServiceState::UNINITIALIZED;
    core::IModule::SlotRange slotRange_ = {};
};

} // namespace cdc::mod_password

extern "C" void mod_password_register();
```

## References

- Related: `PasswordStore.h` contains the actual password storage implementation
- Comparison: See `Fido2Module.h`, `TotpModule.h` for similar module documentation patterns

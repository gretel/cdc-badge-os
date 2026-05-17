---
title: "[MEDIUM] Inconsistent extern \"C\" placement for register functions"
severity: MEDIUM
domain: code-quality/consistency
lens: module-registration
labels:
  - "audit:code-quality/consistency"
---

## Summary
The `extern "C"` declaration for module registration functions is inconsistently placed between header files and source files across modules.

**Pattern A - Declaration in header (correct)**:
- `mod_ble_serial`: `BleSerialModule.h` has declaration
- `mod_fido2`: `Fido2Module.h` has declaration
- `mod_gpg`: `GpgModule.h` has declaration
- `mod_hid`: `HidModule.h` has declaration
- `mod_nvsedit`: `NvsEditModule.h` has declaration
- `mod_password`: `PasswordModule.h` has declaration
- `mod_totp`: `TotpModule.h` has declaration
- `mod_vcard`: `VcardModule.h` has declaration

**Pattern B - Declaration ONLY in source (inconsistent)**:
- `mod_sao`: `SaoModule.h` has NO declaration, only in `SaoModule.cpp`

## Impact
- **Cognitive overhead**: Developers need to check different locations
- **Type safety**: Header declarations enable compile-time checking
- **Documentation**: Headers serve as API documentation
- **Consistency**: Violates the pattern established by other modules

## Evidence
**mod_sao (only module with inconsistency)**:

Header file (`components/mod_sao/include/mod_sao/SaoModule.h`):
```cpp
#pragma once

#include "cdc_core/IModule.h"

namespace cdc::mod_sao {

class SaoModule : public core::IModule {
public:
    static SaoModule& instance();
    const char* getName() const override { return "mod_sao"; }
    // ... no extern "C" void mod_sao_register() declaration
};

} // namespace cdc::mod_sao
```

Source file (`components/mod_sao/src/SaoModule.cpp`):
```cpp
#include "mod_sao/SaoModule.h"
// ...
extern "C" void mod_sao_register() {
    // Registration code
}
```

**Comparison with correct pattern (mod_totp)**:

Header file (`components/mod_totp/include/mod_totp/TotpModule.h`):
```cpp
#pragma once

#include "cdc_core/IModule.h"

namespace cdc::mod_totp {

class TotpModule : public core::IModule {
public:
    static TotpModule& instance();
    // ...
};

} // namespace cdc::mod_totp

extern "C" void mod_totp_register();  // Declaration in header
```

## Recommended Fix
Add the `extern "C"` declaration to `mod_sao/SaoModule.h`:

```cpp
// components/mod_sao/include/mod_sao/SaoModule.h
#pragma once

#include "cdc_core/IModule.h"

namespace cdc::mod_sao {

class SaoModule : public core::IModule {
public:
    static SaoModule& instance();
    const char* getName() const override { return "mod_sao"; }
    const char* getVersion() const override { return "1.0.0"; }
    core::ServiceState getState() const override { return state_; }
    bool init() override;
    bool start() override;
    void stop() override;
    uint8_t getMenuItems(core::ModuleMenuItem* items, uint8_t maxItems) override;
    void onUnlock() override;

private:
    core::ServiceState state_ = core::ServiceState::UNINITIALIZED;
};

} // namespace cdc::mod_sao

extern "C" void mod_sao_register();  // Add this line
```

## References
- Main CMakeLists.txt generates extern declarations from a list
- All other modules follow the header declaration pattern
- Consistent with C++ best practices for C linkage

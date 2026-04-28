---
title: "[MEDIUM] Business logic mixed with UI presentation in TOTP module"
severity: MEDIUM
domain: architecture
lens: separation-of-concerns
labels:
  - "audit:architecture/separation-of-concerns"
---

## Summary
The TOTP module (`components/mod_totp/src/TotpModule.cpp`) mixes business logic (code generation, account storage) directly with UI presentation logic (rendering, view management) in the same class/file. The `TotpCodeView` class embedded within the module handles both domain operations (`generateCode`, `readAccount`) and UI rendering (`render`, `onKey`) simultaneously.

**Location**: `components/mod_totp/src/TotpModule.cpp:331-530` (TotpCodeView class)

## Impact
- **Maintainability**: Changing UI rendering requires touching business logic code
- **Testability**: Business logic cannot be tested without UI dependencies
- **Reusability**: The TOTP code generation logic is tightly coupled to the specific view implementation
- **Single Responsibility**: The module class handles storage, code generation, serial commands, AND UI view construction

## Evidence
```cpp
// Line 331-530: TotpCodeView class embedded in module
class TotpCodeView : public ui::ViewBase {
public:
    void render(bool partial) override {
        // ... UI rendering code ...
        // Line 500-525: Business logic mixed in render/onTick
        updateCode();  // Calls store.generateCode(), store.readAccount()
    }
    
    void updateCode() {
        TotpStore& store = TotpStore::instance();
        store.generateCode(slot_, code_);  // Business logic
        store.readAccount(slot_, &account); // Data access
    }
};
```

The module also handles:
- Serial command registration (lines 96-328)
- UI view management (lines 540-900)
- Business logic (TOTP generation via TotpStore)

## Recommended Fix
1. **Extract business logic** into a standalone `TotpService` class:
   - `generateCode(slot, code)` 
   - `readAccount(slot, account)`
   - `isTimeValid()`
   
2. **Create dedicated view model** for UI:
   - `TotpCodeViewModel` that holds display state (code, remaining, issuer)
   - View queries model for data, model calls service

3. **Refactor `TotpCodeView`** to depend on service/model:
   ```cpp
   class TotpCodeView : public ui::ViewBase {
       TotpService& service_;  // Dependency injection
       TotpCodeViewModel model_;
       void updateCode() { model_ = service_.getCodeForSlot(slot_); }
   };
   ```

## References
- [SRP - Single Responsibility Principle](https://en.wikipedia.org/wiki/Single_responsibility_principle)
- [Model-View-Controller pattern](https://en.wikipedia.org/wiki/Model%E2%80%93view%E2%80%93controller)
- Related issue: Password module has same pattern (`components/mod_password/src/PasswordModule.cpp`)

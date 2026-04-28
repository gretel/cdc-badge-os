---
title: "[LOW] TotpStore and PasswordStore use static singleton pattern with no state reset capability"
severity: LOW
domain: State Management Architecture
lens: state-architecture
labels:
  - "audit:architecture/state-architecture"
---

## Summary
Both `TotpStore` (`components/mod_totp/src/TotpStore.cpp`) and `PasswordStore` (`components/mod_password/src/PasswordStore.cpp`) use static singleton pattern with module-specific state (`hasSlotRange_`, `rmemStart_`, `rmemEnd_`, `moduleId_`). Once initialized, there is no way to reset or reconfigure the state without reboot.

### Evidence:
- `TotpStore.cpp:78-83`: `TotpStore::instance()` returns static singleton
- `TotpStore.h:45-50`: State members `hasSlotRange_`, `rmemStart_`, `rmemEnd_`, `moduleId_`
- `TotpStore.cpp:92-103`: `setSlotRange()` sets state but no `resetSlotRange()` to clear
- `PasswordStore.cpp:49-54`: Same pattern for `PasswordStore::instance()`

### State Access Pattern:
```cpp
// Module gets slot range from registry
store.setSlotRange(start, end, moduleId);

// But no way to reset:
// - Can't clear slot range
// - Can't reconfigure without reboot
// - Can't check current slot state
```

## Impact
1. **No reconfiguration**: If module needs to change slot range, must reboot
2. **No state query**: Can't check current slot configuration for debugging
3. **Test difficulty**: Static singleton persists across test runs

## Recommended Fix
1. Add reset capability:
   ```cpp
   void resetSlotRange() {
       hasSlotRange_ = false;
       rmemStart_ = 0;
       rmemEnd_ = 0;
       moduleId_ = 0;
   }
   ```

2. Add state query:
   ```cpp
   typedef struct {
       bool hasSlotRange;
       uint16_t rmemStart;
       uint16_t rmemEnd;
       uint8_t moduleId;
       uint16_t capacity;
   } StoreState;
   
   StoreState getState() const;
   ```

3. Add slot status check:
   ```cpp
   bool isSlotUsed(uint16_t slot) const;
   uint16_t getUsedCount() const;
   ```

## References
- `components/mod_totp/include/mod_totp/TotpStore.h` - TOTP store header
- `components/mod_totp/src/TotpStore.cpp` - TOTP store implementation
- `components/mod_password/include/mod_password/PasswordStore.h` - Password store header
- `components/mod_password/src/PasswordStore.cpp` - Password store implementation

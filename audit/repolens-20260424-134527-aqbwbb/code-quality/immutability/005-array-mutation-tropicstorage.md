---
title: "[LOW] Array mutation patterns in storage modules"
severity: LOW
domain: code-quality
lens: immutability
labels:
  - "audit:code-quality/immutability"
---

## Summary
The codebase uses array mutation patterns (push, clear, erase) that could benefit from immutable alternatives or better encapsulation. While these patterns are appropriate for the use case (storage management), documenting the mutation patterns and considering immutable views would improve code clarity.

**Files affected:**
- `components/mod_totp/src/TotpStore.cpp`: Uses `eraseSlot()` for array manipulation
- `components/mod_password/src/PasswordStore.cpp`: Uses `eraseSlot()` for array manipulation
- `components/mod_totp/src/TotpModule.cpp`: Array operations for account management
- `components/mod_gpg/src/GpgStorage.cpp`: Storage slot management

## Impact
- **Predictability**: Mutable arrays make it harder to reason about state at any given point
- **Thread safety**: Array mutations without synchronization can cause issues in multi-core context
- **Testing**: Mutable state requires careful test setup/teardown
- **API design**: Methods that mutate arrays in-place vs. returning new arrays have different semantics

## Evidence
From the grep results:
```
/components/mod_totp/src/TotpStore.cpp:    cdc::core::TropicStorage::instance().eraseSlot(moduleId_, physSlot);
/components/mod_password/src/PasswordStore.cpp:    cdc::core::TropicStorage::instance().eraseSlot(moduleId_, physSlot);
/components/mod_totp/src/TotpModule.cpp:        core::ModuleRegistry::instance().clearModuleErrorByName(getName());
```

The `TropicStorage` class appears to use mutable operations like `eraseSlot()` and `clearModuleErrorByName()` that modify internal state.

## Recommended Fix
1. **Document mutation patterns**: Add comments indicating when methods mutate state
   ```cpp
   /**
    * \brief Erases a slot (mutates internal storage).
    * \param moduleId Module ID.
    * \param physSlot Physical slot index.
    * \note This method modifies internal state.
    */
   void eraseSlot(uint8_t moduleId, uint16_t physSlot);
   ```

2. **Consider return values**: Methods that mutate could return the new state or a status
   ```cpp
   // Instead of void
   bool eraseSlot(uint8_t moduleId, uint16_t physSlot);
   ```

3. **Provide immutable views**: If arrays are frequently read, provide const accessors
   ```cpp
   const std::array<Account, MAX_ACCOUNTS>& getAccounts() const;
   // vs.
   std::array<Account, MAX_ACCOUNTS>& getAccounts();
   ```

4. **Batch mutations**: For multiple operations, consider a transaction pattern
   ```cpp
   class StorageTransaction {
   public:
       void eraseSlot(uint16_t slot);
       void commit();
       void rollback();
   };
   ```

This is a LOW severity issue because:
- The mutation patterns are appropriate for storage management
- The code works correctly
- The main benefit is improved documentation and API clarity

## References
- Functional programming principles applied to C++
- "Effective Modern C++" by Scott Meyers, Item 41: Replace mutable state with immutable where possible

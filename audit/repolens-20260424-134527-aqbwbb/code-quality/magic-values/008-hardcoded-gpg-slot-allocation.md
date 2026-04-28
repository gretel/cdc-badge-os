---
title: "[MEDIUM] Hardcoded R-Memory slot numbers in GPG storage"
severity: MEDIUM
domain: code-quality
lens: magic-values
labels:
  - "audit:code-quality/magic-values"
---

## Summary
Magic value `0` is used as the R-Memory slot index for the DEC key in `components/mod_gpg/src/GpgStorage.cpp`. While defined as a constant (`RMEM_SLOT_DEC_KEY = 0`), the slot allocation strategy across all modules should be centralized and documented.

**Location:** `components/mod_gpg/src/GpgStorage.cpp:28`

## Impact
- **Maintainability**: Slot allocations are documented in `CLAUDE.md` but implemented individually in each module.
- **Consistency**: Different modules may use different strategies for slot allocation.
- **Collision Risk**: Without a centralized allocation system, two modules might use the same slot.

## Evidence

**Line 28:**
```cpp
/** \brief R-Memory slot index used for DEC key payload within module range. */
static constexpr uint16_t RMEM_SLOT_DEC_KEY = 0;
```

**Lines 318-320:**
```cpp
// Calculate R-Memory slot early to avoid goto crossing initialization
uint16_t rmem_slot = s_storage.rmemStart + RMEM_SLOT_DEC_KEY;
```

Note: The slot is calculated relative to `s_storage.rmemStart`, which is set by `gpg_storage_set_rmem_range()`. This means the actual slot depends on runtime configuration.

## Recommended Fix

1. Centralize R-Memory slot allocations in a shared header:
```cpp
// components/mod_gpg/include/mod_gpg/gpg_slots.h
/** \brief GPG module R-Memory slot assignments (relative to module base). */
static constexpr uint16_t GPG_RMEM_SLOT_DEC_KEY = 0;   // DEC private key
static constexpr uint16_t GPG_RMEM_SLOT_COUNT = 1;     // Total R-Memory slots used
```

2. Add a module registration system for slot allocation:
```cpp
// components/cdc_core/include/cdc_core/SlotAllocator.h
class SlotAllocator {
public:
    static SlotAllocator& instance();
    uint16_t allocateRMem(const char* module, uint16_t count);
    uint16_t allocateECC(const char* module, uint16_t count);
};
```

3. Document the complete slot map in code:
```cpp
/**
 * \brief TROPIC01 R-Memory Slot Allocation
 *
 * | Range     | Module      | Usage                    |
 * |-----------|-------------|--------------------------|
 * | 0         | SYSTEM      | PINs (Badge, PW1, PW3)   |
 * | 1-3       | GPG         | Key metadata             |
 * | 4         | CA          | Certificate metadata     |
 * | 5-31      | FIDO2       | Credential metadata      |
 * | 32-131    | TOTP        | TOTP secrets (100)       |
 * | 132-149   | Password    | Vault entries (18)       |
 * | 150-511   | Password    | Password entries (362)   |
 */
```

## References
- `CLAUDE.md` - Module Architecture section
- `components/mod_gpg/src/GpgStorage.cpp` - GPG storage implementation
- `components/cdc_core/include/cdc_core/TropicStorage.h` - TROPIC01 storage abstraction

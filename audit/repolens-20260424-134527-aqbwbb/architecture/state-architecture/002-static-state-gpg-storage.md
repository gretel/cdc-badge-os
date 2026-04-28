---
title: "[MEDIUM] GPG storage uses mutable static state with unclear initialization lifecycle"
severity: MEDIUM
domain: State Management Architecture
lens: state-architecture
labels:
  - "audit:architecture/state-architecture"
---

## Summary
The GPG storage module (`components/mod_gpg/src/GpgStorage.cpp`) uses a static struct `s_storage` (line 65-79) to hold slot configuration and session state. The state is set via `gpg_storage_set_slot_range()` and `gpg_storage_set_rmem_range()` but there is no initialization order guarantee, no validation of state transitions, and no way to query the full state for debugging.

### Evidence:
- `GpgStorage.cpp:65-79`: `s_storage` struct with `ready`, `eccStart`, `eccEnd`, `sigSlot`, `decSlot`, `autSlot`, `sessionActive`, `sessionKey`
- `GpgStorage.cpp:169-188`: `gpg_storage_set_slot_range()` sets state without validation
- `GpgStorage.cpp:190-198`: `gpg_storage_set_rmem_range()` sets state separately (potential for inconsistent state)
- `GpgStorage.cpp:201-204`: `gpg_storage_ready()` only returns `s_storage.ready` - no other state query functions

## Impact
1. **Partial initialization risk**: `set_slot_range()` can be called before `set_rmem_range()`, leaving `s_storage.ready = false` but partial state set
2. **Session key lifecycle**: `s_storage.sessionKey` persists after PIN verification but there's no explicit timeout or clear function exposed
3. **No state inspection**: Debugging requires knowing to check `s_storage` directly; no `gpg_storage_get_state()` API
4. **Module coupling**: GPG module must call both set functions in correct order; no validation helper

## Recommended Fix
1. Add state query function:
   ```cpp
   typedef struct {
       bool ready;
       uint16_t eccStart;
       uint16_t eccEnd;
       uint16_t rmemStart;
       uint16_t rmemEnd;
       uint8_t sigSlot;
       uint8_t decSlot;
       uint8_t autSlot;
       bool sessionActive;
   } GpgStorageState;
   
   GpgStorageState gpg_storage_get_state(void);
   ```

2. Add combined initialization:
   ```cpp
   bool gpg_storage_init(uint16_t eccStart, uint16_t eccEnd, uint16_t rmemStart, uint16_t rmemEnd);
   ```

3. Add explicit session clear:
   ```cpp
   void gpg_storage_clear_session_key(void);  // Clear sessionKey, set sessionActive=false
   ```

4. Add validation in `gpg_storage_set_rmem_range()`:
   - Check `s_storage.ready` is already set
   - Validate `rmemStart <= rmemEnd`

## References
- `components/mod_gpg/src/GpgStorage.cpp:65-79` - `s_storage` struct definition
- `components/mod_gpg/src/GpgStorage.cpp:169-198` - Slot range setters
- `components/mod_gpg/include/mod_gpg/GpgStorage.h` - Public API

---
title: "[MEDIUM] PIN storage creates coupling between GPG and FIDO2 modules"
severity: MEDIUM
domain: architecture/coupling
lens: pin-storage-coupling
labels:
  - "audit:architecture/coupling"
---

## Summary
The `pin_storage.h` header creates coupling between GPG and FIDO2 modules. Both modules include and use the same PIN storage functions, which access the same TROPIC01 R-Memory slot 0. This creates implicit coupling: if one module changes the PIN format, the other module breaks.

**Evidence:**
- `components/mod_gpg/include/pin_storage.h` (lines 10-21): PIN storage interface
- `components/mod_gpg/src/GpgModule.cpp` (line 16): Includes `pin_storage.h`
- `components/mod_fido2/src/ctap2.cpp` (line 24): Includes `mod_fido2/pin_storage.h`
- Both modules use the same `PinManager` singleton (R-Memory slot 0)

## Impact
**Shared mutable state:** Both GPG and FIDO2 access the same PIN storage. If GPG changes the PIN, FIDO2 sees the change.

**Format coupling:** The PIN storage format (magic byte 0xDD, hash sizes, etc.) is shared. If one module changes the format, the other breaks.

**No isolation:** GPG and FIDO2 should be independent, but they're coupled through PIN storage.

**Example:**
```cpp
// GPG module uses PINs for OpenPGP
pin_storage_openpgp_verify_pw1("123456");  // User PIN

// FIDO2 module uses the same PIN storage for FIDO2 PIN
// Both share R-Memory slot 0!
```

## Evidence
**File: `components/mod_gpg/include/pin_storage.h` (lines 10-21)**
```cpp
void pin_storage_openpgp_init(void);
bool pin_storage_openpgp_verify_pw1(const char *pin);
bool pin_storage_openpgp_verify_pw3(const char *pin);
bool pin_storage_openpgp_change_pw1(const char *new_pin);
bool pin_storage_openpgp_change_pw3(const char *new_pin);
uint8_t pin_storage_openpgp_pw1_retries(void);
uint8_t pin_storage_openpgp_pw3_retries(void);
void pin_storage_openpgp_reset_pw1_retries(void);
void pin_storage_openpgp_reset_pw3_retries(void);
bool pin_storage_openpgp_pw1_blocked(void);
bool pin_storage_openpgp_pw3_blocked(void);
bool pin_storage_openpgp_reset(void);
```

**File: `components/mod_gpg/src/GpgModule.cpp` (line 16)**
```cpp
#include "pin_storage.h"
```

**File: `components/mod_fido2/src/ctap2.cpp` (line 24)**
```cpp
#include "mod_fido2/pin_storage.h"
```

**File: `components/cdc_core/include/cdc_core/PinManager.h` (lines 30-40)**
```cpp
/**
 * PIN Manager - Manages all device PINs in TROPIC01 R-Memory Slot 0
 *
 * Storage Format (106 bytes):
 * [Magic 0xDD]           (1)  - Format identifier
 * [Badge/FIDO2 Hash]     (16) - LEFT(SHA256(PIN), 16)
 * [Badge Retries]        (1)  - Remaining attempts for Badge PIN
 * ...
 */
class PinManager {
```

## Recommended Fix
**Option 1: Separate PIN slots**
Give GPG and FIDO2 separate R-Memory slots for PIN storage:
- GPG: R-Memory slot 1
- FIDO2: R-Memory slot 2

**Option 2: Unified PIN manager**
Use `PinManager` singleton for all PINs. GPG and FIDO2 call `PinManager::verifyPW1()` instead of `pin_storage_openpgp_verify_pw1()`.

**Option 3: PIN abstraction layer**
Create a `PINProvider` interface. GPG and FIDO2 implement their own PIN providers.

## References
- `components/mod_gpg/include/pin_storage.h` - GPG PIN storage
- `components/mod_fido2/src/ctap2.cpp` - FIDO2 PIN storage
- `components/cdc_core/include/cdc_core/PinManager.h` - Unified PIN manager
- `main/tropic_slot_map.h` - Slot allocation

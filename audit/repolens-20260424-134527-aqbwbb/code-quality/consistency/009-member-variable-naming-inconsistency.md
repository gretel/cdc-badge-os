---
title: "[MEDIUM] Member Variable Naming Convention Inconsistency"
severity: MEDIUM
domain: code-style
lens: code-consistency
labels:
  - "audit:code-quality/consistency"
---

## Summary
The codebase uses inconsistent naming conventions for member variables:

1. **cdc_core**: Trailing underscore suffix (e.g., `badgeHash_`, `valid_`, `errorMessage_`)
2. **CalEPD**: Leading underscore prefix (e.g., `_buffer`, `_initial`, `_debug_buffer`)

### Evidence

**cdc_core (trailing underscore - consistent):**
```cpp
// components/cdc_core/include/cdc_core/PinManager.h
uint8_t badgeHash_[BADGE_HASH_SIZE] = {};
uint8_t badgeRetries_ = MAX_RETRIES;

// components/cdc_core/include/cdc_core/TropicSlotMap.h
bool valid_ = true;
const char* errorMessage_ = nullptr;

// components/cdc_core/include/cdc_core/AttestationKeyService.h
bool ready_ = false;
```

**CalEPD (leading underscore - mixed):**
```cpp
// components/CalEPD/include/gdep015OC1.h
uint8_t _buffer[GDEP015OC1_BUFFER_SIZE];
bool color = false;       // No prefix!
bool _initial = true;
bool _debug_buffer = false;  // snake_case with leading underscore

// components/CalEPD/include/gdew075HD.h
uint8_t _buffer[GDEW075HD_BUFFER_SIZE];
bool _using_partial_mode = false;  // snake_case with leading underscore
bool _initial = true;

// components/CalEPD/include/parallel/ED060SC4.h
bool _tempalert = false;   // camelCase with leading underscore
bool _initial = true;
bool _debug_buffer = false;

// components/CalEPD/include/parallel/ED047TC1touch.h
bool color = false;        // No prefix!
bool _initial = true;
```

**Key inconsistencies:**
- Trailing underscore (cdc_core) vs. Leading underscore (CalEPD)
- Within CalEPD: `_debug_buffer` (snake_case) vs. `_tempalert` (camelCase)
- Some CalEPD members have no prefix: `color` vs. `_initial`
- No consistent pattern for boolean members: `color`, `_initial`, `_tempalert`

## Impact
- **Cognitive overhead**: Developers must remember which convention applies where
- **Search difficulty**: Harder to grep for all member variables
- **Code reviews**: Inconsistent naming distracts from actual changes
- **Maintenance friction**: New contributors unsure which convention to follow

## Recommended Fix

**Establish and document a single convention:**

**Option A: Adopt trailing underscore (cdc_core style)**
- Consistent with ESP-IDF convention
- Matches existing cdc_core codebase
- Easier to read in method signatures

**Option B: Adopt leading underscore (CalEPD style)**
- Common in other C++ codebases
- Clear visual distinction from local variables

### Recommended: Option A (trailing underscore)

**Files to fix in CalEPD (scope for ~1 hour fix):**
- `components/CalEPD/include/gdep015OC1.h`
- `components/CalEPD/include/gdew075HD.h`
- `components/CalEPD/include/parallel/ED060SC4.h`
- `components/CalEPD/include/parallel/ED047TC1.h`
- `components/CalEPD/include/gdew0213i5f.h`

### Rename pattern:
```cpp
// BEFORE (CalEPD style - leading underscore)
uint8_t _buffer[GDEW075HD_BUFFER_SIZE];
bool _using_partial_mode = false;
bool _initial = true;
bool _debug_buffer = false;
bool color = false;       // No prefix!
bool _tempalert = false;

// AFTER (cdc_core style - trailing underscore)
uint8_t buffer_[GDEW075HD_BUFFER_SIZE];
bool usingPartialMode_ = false;  // camelCase with trailing underscore
bool initial_ = true;
bool debugBuffer_ = false;
bool color_ = false;
bool tempAlert_ = false;
```

### Consistent naming pattern:
```cpp
// Member variables: camelCase with trailing underscore
uint8_t buffer_[SIZE];
bool enabled_ = false;
uint16_t count_ = 0;
const char* name_ = nullptr;
```

## References
- ESP-IDF Style Guide: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/contribute/style-guide.html
- C++ Core Guidelines: Naming rules
- cdc-badge-os project convention: Trailing underscore used in cdc_core, cdc_ui, cdc_hal

</content>
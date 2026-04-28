---
title: "[MEDIUM] Enum Declaration Style Inconsistency: typedef enum vs enum class"
severity: MEDIUM
domain: code-style
lens: code-consistency
labels:
  - "audit:code-quality/consistency"
---

## Summary
The codebase uses inconsistent enum declaration styles:

1. **Most cdc modules**: `enum class` (scoped enums with type)
2. **cdc_log, mod_fido2, mod_vcard**: `typedef enum` (C-style enums)

### Evidence

**C-style typedef enum (inconsistent):**

```cpp
// components/cdc_log/include/cdc_log.h
typedef enum {
    CDC_LOG_LEVEL_NONE = 0,
    CDC_LOG_LEVEL_ERROR,
    CDC_LOG_LEVEL_WARN,
    CDC_LOG_LEVEL_INFO,
    CDC_LOG_LEVEL_DEBUG,
} cdc_log_level_t;

// components/mod_fido2/include/mod_fido2/fido2.h
typedef enum {
    FIDO2_UP_PENDING = 0,   // Waiting for user
    FIDO2_UP_APPROVED,      // User approved
    FIDO2_UP_DENIED,        // User denied
} fido2_user_presence_t;

typedef enum {
    FIDO2_ACTION_REGISTER = 0,  // makeCredential
    FIDO2_ACTION_AUTHENTICATE,  // getAssertion
    FIDO2_ACTION_SELECT         // Browser probe
} fido2_action_t;

// components/mod_vcard/include/mod_vcard/ble_vcard.h
typedef enum {
    VCARD_EXCHANGE_IDLE = 0,
    VCARD_EXCHANGE_CONNECTING,
    VCARD_EXCHANGE_DISCOVERING,
    VCARD_EXCHANGE_ACTIVE,
    VCARD_EXCHANGE_INACTIVE,
} vcard_exchange_state_t;
```

**Modern enum class (consistent with cdc_core):**

```cpp
// components/cdc_core/include/cdc_core/IService.h
enum class ServiceState : uint8_t {
    UNINITIALIZED,
    INITIALIZED,
    STARTED,
};

// components/cdc_core/include/cdc_core/EventBus.h
enum class EventType : uint8_t {
    KEY_PRESSED,
    KEY_RELEASED,
};

// components/mod_totp/include/mod_totp/TotpStore.h
enum class TotpAlgorithm : uint8_t {
    SHA1,
    SHA256,
    SHA512,
};
```

**Key differences:**
- `typedef enum` creates a C-style unscoped enum with typedef
- `enum class` creates a C++-style scoped enum with explicit underlying type
- `typedef enum` values are in global scope (e.g., `CDC_LOG_LEVEL_ERROR`)
- `enum class` values are scoped (e.g., `ServiceState::STARTED`)
- `typedef enum` allows implicit conversion to int
- `enum class` requires explicit cast for arithmetic

## Impact
- **Type safety**: `enum class` provides better type checking
- **Namespace pollution**: `typedef enum` values pollute enclosing scope
- **Modern C++**: `enum class` is preferred in C++11 and later
- **Readability**: `enum class` makes enum origin explicit
- **Consistency**: Mixed styles make code harder to read and maintain

## Recommended Fix

**Establish and document a single convention:**

1. **Adopt `enum class`** (consistent with cdc_core and modern C++):
   - Better type safety
   - Scoped names (no pollution)
   - Explicit underlying type

2. **Files to fix (scope for ~1 hour fix):**
   - `components/cdc_log/include/cdc_log.h`
   - `components/mod_fido2/include/mod_fido2/fido2.h`
   - `components/mod_vcard/include/mod_vcard/ble_vcard.h`

### Rename pattern:

```cpp
// BEFORE (C-style typedef enum)
typedef enum {
    CDC_LOG_LEVEL_NONE = 0,
    CDC_LOG_LEVEL_ERROR,
    CDC_LOG_LEVEL_WARN,
} cdc_log_level_t;

// AFTER (C++-style enum class)
enum class LogLevel : uint8_t {
    NONE = 0,
    ERROR,
    WARN,
};

// Usage:
// BEFORE: cdc_log_level_t level = CDC_LOG_LEVEL_ERROR;
// AFTER: LogLevel level = LogLevel::ERROR;
```

### For fido2.h:

```cpp
// BEFORE
typedef enum {
    FIDO2_UP_PENDING = 0,
    FIDO2_UP_APPROVED,
    FIDO2_UP_DENIED,
} fido2_user_presence_t;

// AFTER
enum class UserPresence : uint8_t {
    PENDING = 0,
    APPROVED,
    DENIED,
};
```

## References
- C++ Core Guidelines: Use `enum class` instead of `enum`
- Modern C++: `enum class` is preferred since C++11
- cdc-badge-os project convention: `enum class` used in cdc_core, cdc_ui, cdc_hal
- [cppreference: Scoped enums](https://en.cppreference.com/w/cpp/language/enum)

</content>
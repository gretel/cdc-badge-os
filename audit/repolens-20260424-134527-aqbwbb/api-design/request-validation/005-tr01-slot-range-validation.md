---
title: "[LOW] TR01_RMEM_READ and TR01_ECC_DEL accept unvalidated slot numbers"
severity: LOW
domain: api-design/request-validation
lens: serial-command-interface
labels:
  - "request-validation"
  - "serial-commands"
  - "secure-element"
---

## Summary

The TROPIC01 secure element commands (`TR01_RMEM_READ`, `TR01_ECC_DEL`, `TR01_RMEM_DEL`) in `components/serial_cmd/src/SerialCmd.cpp` use a helper function `parseSlotArg()` for validation, but the validation logic uses `strtol` which can parse very large numbers and then checks against bounds. A more robust approach would validate the string format first before conversion.

**Location:** `components/serial_cmd/src/SerialCmd.cpp:162-180` (parseSlotArg), `993-1010` (TR01_RMEM_READ), `1012-1029` (TR01_ECC_DEL), `1031-1048` (TR01_RMEM_DEL)

## Impact

- **Minor parsing edge cases**: Very large numbers like `"99999999999999999999"` would be parsed by `strtol` and then rejected, causing overflow warnings
- **Consistency**: Other commands use more explicit parsing approaches

## Evidence

```cpp
// File: components/serial_cmd/src/SerialCmd.cpp:152-180
static SlotParseResult parseSlotArg(const char* args, uint16_t maxSlot, const char* slotTypeName) {
    SlotParseResult result = {false, 0};

    if (!args || !*args) {
        Console::printf("Usage: Provide a %s number\r\n", slotTypeName);
        return result;
    }

    char* endptr = nullptr;
    long slotVal = strtol(args, &endptr, 10);  // Line 165

    if (endptr == args || *endptr != '\0' || slotVal < 0) {  // Line 167
        Console::printf("ERROR: Invalid %s number\r\n", slotTypeName);
        return result;
    }

    if (slotVal >= maxSlot) {  // Line 172
        Console::printf("ERROR: Invalid %s (0-%d)\r\n", slotTypeName, maxSlot);
        return result;
    }

    result.valid = true;
    result.value = slotVal;
    return result;
}
```

Issues:
1. `strtol` can overflow for very large numbers before the range check
2. No check if `slotVal` exceeds `LONG_MAX` or `LONG_MIN`
3. No check if the parsed value exceeds `uint16_t` range before casting

## Recommended Fix

Add overflow checking to `parseSlotArg()`:

```cpp
static SlotParseResult parseSlotArg(const char* args, uint16_t maxSlot, const char* slotTypeName) {
    SlotParseResult result = {false, 0};

    if (!args || !*args) {
        Console::printf("Usage: Provide a %s number\r\n", slotTypeName);
        return result;
    }

    char* endptr = nullptr;
    long slotVal = strtol(args, &endptr, 10);

    if (endptr == args || *endptr != '\0' || slotVal < 0) {
        Console::printf("ERROR: Invalid %s number\r\n", slotTypeName);
        return result;
    }
    
    // Check for overflow before casting
    if (slotVal > USHRT_MAX) {
        Console::printf("ERROR: %s number too large\r\n", slotTypeName);
        return result;
    }

    if (slotVal >= maxSlot) {
        Console::printf("ERROR: Invalid %s (0-%d)\r\n", slotTypeName, maxSlot - 1);
        return result;
    }

    result.valid = true;
    result.value = static_cast<long>(slotVal);
    return result;
}
```

Or alternatively, use a more robust parsing approach:

```cpp
static SlotParseResult parseSlotArg(const char* args, uint16_t maxSlot, const char* slotTypeName) {
    SlotParseResult result = {false, 0};

    if (!args || !*args) {
        Console::printf("Usage: Provide a %s number\r\n", slotTypeName);
        return result;
    }

    // Validate all characters are digits
    const char* p = args;
    while (*p && isspace(*p)) p++;  // Skip leading whitespace
    if (!*p) {
        Console::printf("Usage: Provide a %s number\r\n", slotTypeName);
        return result;
    }
    
    long slotVal = 0;
    int digits = 0;
    while (*p && !isspace(*p)) {
        if (!isdigit(*p)) {
            Console::printf("ERROR: Invalid %s number\r\n", slotTypeName);
            return result;
        }
        slotVal = slotVal * 10 + (*p - '0');
        digits++;
        p++;
    }
    
    // Check for too many digits (uint16_t max is 65535 = 5 digits)
    if (digits > 5) {
        Console::printf("ERROR: %s number too large\r\n", slotTypeName);
        return result;
    }

    if (slotVal >= maxSlot) {
        Console::printf("ERROR: Invalid %s (0-%d)\r\n", slotTypeName, maxSlot - 1);
        return result;
    }

    result.valid = true;
    result.value = slotVal;
    return result;
}
```

## References

- CWE-190: Integer overflow or wraparound
- CWE-20: Improper Input Validation
- TROPIC01 datasheet for slot limits

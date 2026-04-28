---
title: "[LOW] Switch statements missing default case handling"
severity: LOW
domain: linting
lens: code-quality/linting
labels:
  - "audit:code-quality/linting"
---

## Summary
Several switch statements in the codebase have default cases that don't handle unknown enum values explicitly. While most have a default, some just silently ignore unknown values without logging or error handling.

**Location:** Multiple component files

## Impact
- **Silent failures**: Unknown enum values may be ignored without any indication
- **Debugging difficulty**: Hard to detect when new enum values are added but switch statements aren't updated
- **Missing logging**: No trace of unexpected values for troubleshooting

## Evidence

### File: `components/cdc_views/src/ListView.cpp:164`
```cpp
InputResult ListView::onKey(char key) {
    switch (key) {
        case '2': // Up
            navigate(false);
            return InputResult::CONSUMED;

        case '8': // Down
            navigate(true);
            return InputResult::CONSUMED;

        case 'Y': // Select
            if (onSelect_ && items_ && selection_ < itemCount_) {
                onSelect_(selection_, items_[selection_].userData);
            }
            return InputResult::CONSUMED;

        case '3': // Context menu
            // ...

        default:  // Just falls through, no logging
            return InputResult::IGNORED;
    }
}
```

### File: `components/cdc_views/src/PinEntryView.cpp:209`
```cpp
switch (key) {
    case 'N': // Backspace or cancel
        if (length_ > 0) {
            backspace();
        } else {
            if (onCancel_) {
                onCancel_();
                return InputResult::CONSUMED;
            }
            return InputResult::REQUEST_POP;
        }
        return InputResult::CONSUMED;

    case 'Y': // Confirm
        verify();
        return InputResult::CONSUMED;

    default:  // Just falls through, no logging
        // Check if key is a digit
        if (key >= '0' && key <= '9') {
            append(key - '0');
            return InputResult::CONSUMED;
        }
        return InputResult::IGNORED;
}
```

### File: `components/mod_fido2/src/ctap2.cpp` (multiple locations)
Multiple switch statements in CTAP2 protocol parsing have default cases that just return generic status codes without logging the unexpected value.

## Recommended Fix

Add logging to default cases to help with debugging:

```cpp
// Before:
default:
    return InputResult::IGNORED;

// After:
default:
    LOG_D("ListView", "Unhandled key: '%c' (%d)", key, key);
    return InputResult::IGNORED;
```

For switch statements handling protocol data (like CTAP2), consider logging the unexpected value:

```cpp
// Before:
default:
    return CTAP2_ERR_INVALID_CBOR;

// After:
default:
    LOG_W("CTAP2", "Unexpected CBOR key: 0x%02X", key);
    return CTAP2_ERR_INVALID_CBOR;
```

For simple UI key handlers where logging might be noisy, at least add a comment explaining the default behavior:

```cpp
default:
    // Ignore unknown keys - allow view stack to handle
    return InputResult::IGNORED;
```

## References
- [CWE-478: Missing Default Case in Switch Statement](https://cwe.mitre.org/data/definitions/478.html)
- [C++ Core Guidelines: Switch statements](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#ES40-consider-an-enum-class-rather-than-an-enum)
- [Effective C++ Item 40: Use explicit casts for enum-to-int conversions](https://www.aristeia.com/Book/errata/3rd_edition.html)

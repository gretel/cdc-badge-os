---
title: "[MEDIUM] Use of unsafe C string functions (sprintf, strcpy)"
severity: MEDIUM
domain: linting
lens: code-quality/linting
labels:
  - "audit:code-quality/linting"
---

## Summary
The codebase uses unsafe C string functions (`sprintf`, `strcpy`) that can cause buffer overflows if the source string size is not carefully controlled. While some uses appear safe due to small, known sizes, using bounded alternatives is more robust.

**Location:** Multiple files across the codebase

## Impact
- **Buffer overflow risk**: `sprintf` and `strcpy` don't check destination buffer size
- **Security vulnerability**: If source strings grow unexpectedly, memory corruption can occur
- **Maintainability**: Hard to verify safety without careful code review

## Evidence

### sprintf usage (5 occurrences):

**File:** `components/CalEPD/models/gdeh0213b73.cpp:229`
```cpp
void Gdeh0213b73::cmd(uint8_t command){
  char buffer[3];
  sprintf(buffer,"%x",command);  // Buffer is only 3 chars, hex of uint8_t can be 2 chars + null
  if (gpio_get_level((gpio_num_t)CONFIG_EINK_BUSY) == 1) {
    _waitBusy(buffer);
  }
  IO.cmd(command);
}
```
Risk: `buffer[3]` is tight for hex output of `uint8_t` (max "FF" + null = 3 chars). Borderline safe but fragile.

**File:** `components/mod_fido2/src/ctap2.cpp:639, 1162, 2471`
```cpp
sprintf(hex + (i * 3), "%02X ", response[offset + i]);  // 3 chars per byte: "XX "
```
Risk: Depends on `hex` buffer size being correctly calculated. Harder to verify.

### strcpy usage (3 occurrences):

**File:** `components/cdc_os_ui/src/views/LockScreenView.cpp:68, 127`
```cpp
void LockScreenView::init() {
    memset(name_, 0, sizeof(name_));
    memset(info_, 0, sizeof(info_));
    memset(info2_, 0, sizeof(info2_));
    strcpy(clock_, "--:--");  // Safe: source is literal 5 chars
    ...
}

void LockScreenView::setClock(const char* clock) {
    if (clock) {
        strncpy(clock_, clock, sizeof(clock_) - 1);  // Correct usage
        clock_[sizeof(clock_) - 1] = '\0';
    } else {
        strcpy(clock_, "--:--");  // Safe: source is literal
    }
    ...
}
```

**File:** `components/mod_totp/src/TotpStore.cpp:489`
```cpp
if (!isTimeValid()) {
    strcpy(codeOut, "------");  // Safe: source is literal 6 chars
    return -1;
}
```

## Recommended Fix

### For sprintf:
Replace with `snprintf` with explicit size:

```cpp
// Before:
char buffer[3];
sprintf(buffer, "%x", command);

// After:
char buffer[3];
snprintf(buffer, sizeof(buffer), "%x", command);
```

For the FIDO2 hex formatting:
```cpp
// Before:
sprintf(hex + (i * 3), "%02X ", response[offset + i]);

// After:
snprintf(hex + (i * 3), 4, "%02X ", response[offset + i]);  // 4 to be safe
```

### For strcpy:
Either use `strncpy` with null termination, or `snprintf`:

```cpp
// Before:
strcpy(clock_, "--:--");

// After (Option 1 - strncpy):
strncpy(clock_, "--:--", sizeof(clock_) - 1);
clock_[sizeof(clock_) - 1] = '\0';

// After (Option 2 - snprintf):
snprintf(clock_, sizeof(clock_), "--:--");
```

Note: The `LockScreenView::setClock` function already uses `strncpy` correctly for the non-literal case - apply the same pattern consistently.

## References
- [CWE-120: Buffer Overrun with Source Routine](https://cwe.mitre.org/data/definitions/120.html)
- [CppCoreGuidelines: ES.18 - Use the simplest string functions](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#es18-use-the-simplest-string-functions)
- [Safe String Functions](https://learn.microsoft.com/en-us/cpp/c-runtime-library/safe-string-convenience-macros)

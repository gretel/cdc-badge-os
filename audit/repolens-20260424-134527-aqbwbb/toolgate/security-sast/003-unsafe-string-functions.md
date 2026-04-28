---
title: "[MEDIUM] Use of unsafe string functions in core components"
severity: MEDIUM
domain: security
lens: sast
labels:
  - "buffer-overflow"
  - "string-handling"
  - "strcpy"
  - "sprintf"
---

## Summary
The codebase uses unsafe C-style string functions (`strcpy`, `sprintf`) without bounds checking in several places. While some uses are safe due to known buffer sizes, using bounded variants (`strncpy`, `snprintf`) is more robust and prevents potential buffer overflows.

## Impact
- **Buffer Overflow Risk**: `strcpy` and `sprintf` don't check destination buffer size
- **Memory Corruption**: If source strings exceed expected lengths, adjacent memory can be overwritten
- **Stability**: Buffer overflows can cause crashes or unpredictable behavior on embedded systems

## Evidence

**1. `strcpy` in LockScreenView (lines 68, 127):**
File: `components/cdc_os_ui/src/views/LockScreenView.cpp`
```cpp
void LockScreenView::init() {
    memset(name_, 0, sizeof(name_));
    memset(info_, 0, sizeof(info_));
    memset(info2_, 0, sizeof(info2_));
    strcpy(clock_, "--:--");  // Line 68
    memset(date_, 0, sizeof(date_));
    ...
}

void LockScreenView::setClock(const char* clock) {
    if (clock) {
        strncpy(clock_, clock, sizeof(clock_) - 1);
        clock_[sizeof(clock_) - 1] = '\0';
    } else {
        strcpy(clock_, "--:--");  // Line 127
    }
    ...
}
```
Note: Line 68 is safe (static literal), but line 127 could be improved for consistency.

**2. `sprintf` in FIDO2 module (lines 639, 1162, 2471):**
File: `components/mod_fido2/src/ctap2.cpp`
```cpp
sprintf(hex + (i * 3), "%02X ", response[offset + i]);  // Line 639
sprintf(hex + (i * 3), "%02X ", response[offset + i]);  // Line 1162
p += sprintf(p, "%02X ", pin_hash_enc[i + j]);          // Line 2471
```

**3. `sprintf` in CalEPD (lines 229, 185):**
File: `components/CalEPD/models/gdeh0213b73.cpp:229`
```cpp
sprintf(buffer,"%x",command);
```
File: `components/CalEPD/models/fix/gdeh0213b73.cpp:185`
```cpp
sprintf(buffer,"%x",command);
```

**4. `sprintf` in Adafruit-GFX (WString.cpp):**
Multiple uses in `components/Adafruit-GFX/WString.cpp:81,99,361,367,379`

## Recommended Fix

**For `strcpy`:** Replace with `strncpy` + null termination:
```cpp
// Before:
strcpy(clock_, "--:--");

// After:
strncpy(clock_, "--:--", sizeof(clock_) - 1);
clock_[sizeof(clock_) - 1] = '\0';
```

**For `sprintf`:** Replace with `snprintf`:
```cpp
// Before:
sprintf(hex + (i * 3), "%02X ", response[offset + i]);
sprintf(buffer, "%x", command);

// After:
snprintf(hex + (i * 3), 3, "%02X ", response[offset + i]);
snprintf(buffer, sizeof(buffer), "%x", command);
```

**Prioritization:**
1. Fix CalEPD `sprintf` first (unknown buffer size)
2. Fix FIDO2 `sprintf` (large file, verify buffer sizes)
3. Fix LockScreenView `strcpy` for consistency

## References
- CWE-120: Buffer copy without checking size of input
- CWE-134: Use of externally-controlled format string
- OWASP: "Use bounded string functions"

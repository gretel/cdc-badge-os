---
title: "[LOW] Operator Spacing Inconsistency"
severity: LOW
domain: code-style
lens: code-consistency
labels:
  - "audit:code-quality/consistency"
---

## Summary
The codebase uses inconsistent spacing around operators in for loops and expressions:

1. **cdc_core**: Consistent spacing (e.g., `for (uint8_t i = 0; i < count_; i++)`)
2. **CalEPD**: Missing spaces (e.g., `for (int i=0;i<expr;++i)` or `for(i=0;i<48000;i++)`)

### Evidence

**cdc_core (consistent spacing):**
```cpp
// components/cdc_core/src/ModuleRegistry.cpp:45
for (uint8_t i = 0; i < initCount_; i++) {

// components/cdc_core/src/ModuleRegistry.cpp:78
for (uint8_t i = 0; i < count_; i++) {

// components/cdc_core/src/ModuleRegistry.cpp:102
for (uint8_t j = i; j < count_ - 1; j++) {
```

**CalEPD (inconsistent spacing):**
```cpp
// components/CalEPD/models/wave12i48.cpp:90
for (int i=0;i<epd_resolution_m1s2.databytes;++i) {

// components/CalEPD/models/gdew075T7Grays.cpp:184
for(i=0;i<48000;i++)               //48000*4  800*480

// components/CalEPD/models/gdew075T7Grays.cpp:361
for (int i=0; i<size; ++i) {

// components/CalEPD/models/color/dke075z83.cpp:83
for (int i=0;i<sizeof(epd_wakeup_power.data);++i) {

// components/CalEPD/models/gdew075T7Grays.cpp:415
        for(i=0;i<12000;i++)
```

**Key inconsistencies:**
- `i = 0` vs. `i=0` (space around `=`)
- `i < count` vs. `i<count` (space around `<`)
- `i++` vs. `++i` (prefix vs. postfix increment)
- `for (int i` vs. `for(i` (space after `for`)
- Mixed styles within same file (e.g., `i=0; i<size`)

## Impact
- **Visual noise**: Inconsistent spacing makes code harder to scan
- **Git diffs**: Small reformatting changes create noise
- **Code reviews**: Reviewers distracted by formatting instead of logic
- **Readability**: Tight spacing (`i=0;i<expr`) is harder to read

## Recommended Fix

**Establish and document a single convention:**

1. **Adopt consistent spacing** (consistent with cdc_core):
   - Space after `for`: `for (init; cond; inc)`
   - Space around `=`: `i = 0`
   - Space around `<`, `>`, `+`, `-`: `i < count`, `count_ - 1`
   - Space after `;`: `i = 0; i < count`

### Files to fix (scope for ~1 hour fix):
- `components/CalEPD/models/wave12i48.cpp`
- `components/CalEPD/models/gdew075T7Grays.cpp`
- `components/CalEPD/models/color/dke075z83.cpp`
- `components/CalEPD/models/color/gdew075z09.cpp`

### Rename pattern:
```cpp
// BEFORE (missing spaces)
for (int i=0;i<epd_resolution_m1s2.databytes;++i) {
for(i=0;i<48000;i++)

// AFTER (consistent spacing)
for (int i = 0; i < epd_resolution_m1s2.databytes; ++i) {
for (i = 0; i < 48000; i++)
```

## References
- ESP-IDF Style Guide: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/contribute/style-guide.html
- C++ Core Guidelines: Formatting rules
- cdc-badge-os project convention: Consistent spacing used in cdc_core, cdc_ui, cdc_hal

</content>
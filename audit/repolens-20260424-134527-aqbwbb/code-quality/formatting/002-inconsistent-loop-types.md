---
title: "[MEDIUM] Inconsistent loop variable types (int vs size_t) in core components"
severity: MEDIUM
domain: code-quality/formatting
lens: formatting-consistency
labels:
  - "audit:code-quality/formatting"
---

## Summary
The codebase uses inconsistent types for loop counter variables. Within the `cdc_core` component, some files use `size_t` for array/index iteration while others use `int`. This inconsistency makes the code harder to read and maintain.

**Inconsistent usage in `cdc_core`:**
- `ServiceRegistry.cpp`: Uses `size_t` consistently (5 occurrences)
- `TropicSlotMap.cpp`: Uses `size_t` consistently (5 occurrences)  
- `PinManager.cpp`: Uses `size_t` consistently (2 occurrences)
- `KeyFingerprint.cpp`: Uses `int` (1 occurrence at line 59)

## Impact
- **Type safety**: `size_t` is the correct type for array indexing and counts; `int` can overflow
- **Consistency**: Mixed types in the same component create cognitive overhead
- **Warnings**: Using `int` for comparisons with `size_t` (e.g., array lengths) can trigger `-Wsign-compare` warnings

## Evidence

**File: `components/cdc_core/src/KeyFingerprint.cpp:59`**
```cpp
// Uses int (inconsistent with rest of cdc_core)
for (int i = 0; i < KEY_FINGERPRINT_WORD_COUNT; i++) {
```

**File: `components/cdc_core/src/ServiceRegistry.cpp:50`**
```cpp
// Uses size_t (consistent pattern)
for (size_t i = 0; i < count_; i++) {
```

**File: `components/cdc_core/src/TropicSlotMap.cpp:66`**
```cpp
// Uses size_t (consistent pattern)
for (size_t i = 0; i < kSlotMapCount; i++) {
```

## Recommended Fix
Change the loop variable in `KeyFingerprint.cpp` from `int` to `size_t`:

```cpp
for (size_t i = 0; i < KEY_FINGERPRINT_WORD_COUNT; i++) {
```

This aligns with the established pattern in `ServiceRegistry.cpp` and `TropicSlotMap.cpp`.

## References
- C++ Core Guidelines: Use `size_t` for array indexing: https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines
- `size_t` is the standard type for array indices and counts in C/C++

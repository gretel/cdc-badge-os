---
title: "[MEDIUM] Brace Placement and Formatting Inconsistency"
severity: MEDIUM
domain: code-style
lens: code-consistency
labels:
  - "audit:code-quality/consistency"
---

## Summary
The codebase uses inconsistent brace placement and formatting styles between different components:

1. **cdc_core/cdc_ui/cdc_hal (K&R style)**: Opening brace on same line as declaration
2. **CalEPD (Allman-style variations)**: Opening brace on next line, inconsistent spacing

### Evidence

**cdc_core (K&R style - brace on same line):**
```cpp
// components/cdc_core/src/AttestationKeyService.cpp:42
void AttestationKeyService::stop() {
    // ...
}

// components/cdc_core/src/ModuleRegistry.cpp:28
void ModuleRegistry::registerInitializer(ModuleInitFunc initFunc) {
    // ...
}

// components/cdc_core/src/ModuleRegistry.cpp:45
for (uint8_t i = 0; i < initCount_; i++) {
    // ...
}

// components/cdc_core/src/AttestationKeyService.cpp:20
if (state_ != ServiceState::UNINITIALIZED) {
    // ...
}
```

**CalEPD (mixed styles - brace on next line, no spaces):**
```cpp
// components/CalEPD/models/wave12i48.cpp:54
void Wave12I48::_powerOn(){
    // ...
}

// components/CalEPD/models/wave12i48.cpp:64
void Wave12I48::_wakeUp(){
    // ...
}

// components/CalEPD/models/wave12i48.cpp:281
switch (getRotation())
{
    case 1:
      // ...
}

// components/CalEPD/models/wave12i48.cpp:90
for (int i=0;i<epd_resolution_m1s2.databytes;++i) {
    // ...
}
```

**Key differences:**
- cdc_core: `void func() {` (space before brace)
- CalEPD: `void func(){` (no space before brace)
- cdc_core: `for (i = 0; i < n; i++) {` (spaces around operators)
- CalEPD: `for (int i=0;i<expr;++i) {` (no spaces around operators)
- cdc_core: `if (cond) {` (K&R)
- CalEPD: `if (cond)\n{` (Allman-style for some)

## Impact
- **Cognitive overhead**: Developers must switch between formatting styles
- **Harder code reviews**: Inconsistent formatting distracts from actual code changes
- **Git noise**: Reformatting creates large diffs
- **Maintainability**: New contributors unsure which style to follow

## Recommended Fix

**Establish and document a single convention:**

1. **Adopt K&R style** (brace on same line) - consistent with cdc_core and ESP-IDF convention
2. **Standardize spacing**:
   - Space before opening brace: `void func() {`
   - Spaces around operators: `for (int i = 0; i < n; i++) {`
   - Spaces after keywords: `if (cond)`, `for (init; cond; inc)`, `switch (expr)`

3. **Files to reformat in CalEPD** (scope for ~1 hour fix):
   - `components/CalEPD/models/wave12i48.cpp`
   - `components/CalEPD/models/gdem029E97.cpp`
   - `components/CalEPD/models/parallel/ED047TC1.cpp`
   - `components/CalEPD/models/parallel/ED060SC4.cpp`

### Specific changes needed:
```cpp
// BEFORE (CalEPD style)
void Wave12I48::_powerOn(){
  for (int i=0;i<expr;++i) {
  switch (getRotation())
  {
    case 1:

// AFTER (K&R style)
void Wave12I48::_powerOn() {
    for (int i = 0; i < expr; ++i) {
        switch (getRotation()) {
            case 1:
```

## References
- ESP-IDF Style Guide: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/contribute/style-guide.html
- C++ Core Guidelines: Formatting rules
- cdc-badge-os project convention: K&R style used in cdc_core, cdc_ui, cdc_hal

</content>
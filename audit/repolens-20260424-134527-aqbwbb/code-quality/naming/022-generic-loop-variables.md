---
title: "[LOW] Generic loop variable names: excessive use of `i`, `idx`"
severity: LOW
domain: naming-conventions
lens: code-quality
labels:
  - "audit:code-quality/naming"
---

## Summary
The codebase uses generic loop variable names (`i`, `idx`, `count`) extensively. While acceptable for simple loops, more descriptive names could improve readability in complex nested loops.

**Evidence:**

1. **components/cdc_views/src/ListView.cpp** (line 205):
   ```cpp
   for (uint8_t i = 0; i < visibleItems_; i++) {
       uint16_t itemIndex = scrollPos_ + i;
       ...
   }
   ```

2. **components/cdc_core/src/EventBus.cpp** (line 49):
   ```cpp
   for (uint8_t i = 0; i < MAX_HANDLERS; i++) {
       if (!handlers_[i].active) {
   ```

3. **components/serial_cmd/src/CommandRegistry.cpp** (line 46):
   ```cpp
   for (size_t i = 0; i < count_; i++) {
       if (strcasecmp(commands_[i].name, cmd.name) == 0) {
   ```

4. **components/cdc_core/src/EventBus.cpp** (line 123):
   ```cpp
   for (uint8_t i = 0; i < MAX_HANDLERS; i++) {
       if (handlers_[i].active && handlers_[i].handler) {
   ```

## Impact
- **Readability**: For simple loops, `i` is fine
- **Nested loops**: When loops are nested, `i`, `j`, `k` become confusing
- **Context**: In complex loops, the variable's purpose isn't clear from the name

## Evidence
Most loops use simple `i`:
```cpp
for (uint8_t i = 0; i < MAX_HANDLERS; i++) {
for (size_t i = 0; i < count_; i++) {
for (uint8_t i = 0; i < visibleItems_; i++) {
```

## Recommended Fix
Use more descriptive names where it improves clarity:

**For simple single loops:**
```cpp
// OK - simple loop, `i` is fine
for (uint8_t i = 0; i < visibleItems_; i++) {
```

**For loops over collections:**
```cpp
// Better:
for (size_t handlerIdx = 0; handlerIdx < MAX_HANDLERS; handlerIdx++) {
    if (!handlers_[handlerIdx].active) {
```

**For nested loops:**
```cpp
// Better than i, j:
for (size_t row = 0; row < rowCount_; row++) {
    for (size_t col = 0; col < colCount_; col++) {
```

**Steps:**
1. Identify nested loops that need clearer names
2. Rename loop variables to be more descriptive (`handlerIdx`, `itemIdx`, `row`, `col`)
3. Keep simple loops with `i` when clarity isn't affected

## References
- [Google C++ Style Guide - Variable Names](https://google.github.io/styleguide/cppguide.html#Variable_Names)
- [C++ Core Guidelines - Naming](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rc-naming)

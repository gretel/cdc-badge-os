---
title: "[MEDIUM] Boolean member variables lacking clear true/false semantics"
severity: MEDIUM
domain: naming-conventions
lens: code-quality
labels:
  - "audit:code-quality/naming"
---

## Summary
Several boolean member variables use names that don't clearly indicate their true/false semantics. The naming convention in the codebase is inconsistent for boolean members.

**Evidence:**

1. **components/cdc_core/include/cdc_core/PinManager.h** (lines 124-127):
   ```cpp
   bool pinLoaded_ = false;
   bool badgePinIsSet_ = false;
   bool lockoutActive_ = false;
   ```
   Mixed naming: `pinLoaded_` (past participle), `badgePinIsSet_` (is-prefixed), `lockoutActive_` (adjective)

2. **components/cdc_core/include/cdc_core/EventBus.h** (line 142):
   ```cpp
   bool initialized_ = false;
   ```
   Uses past participle.

3. **components/cdc_views/include/cdc_views/ListView.h** (lines 140-143):
   ```cpp
   bool preservePosition_ = false;
   ```
   Uses verb form (imperative).

4. **components/cdc_views/include/cdc_views/PinEntryView.h** (line 136):
   ```cpp
   bool lockedOut_ = false;
   bool showMessages_ = true;
   ```
   Mixed: `lockedOut_` (adjective), `showMessages_` (verb phrase).

5. **components/cdc_core/include/cdc_core/ModuleRegistry.h** (similar patterns):
   ```cpp
   bool started_ = false;
   bool stopped_ = false;
   ```

## Impact
- **Readability**: `preservePosition_` could mean "should preserve" or "is preserving"
- **Consistency**: Different developers follow different patterns
- **Maintenance**: Harder to quickly understand boolean state at a glance

## Evidence
Common patterns found:
- Past participle: `initialized_`, `pinLoaded_`, `started_`, `stopped_`
- `is` prefix: `badgePinIsSet_`
- Adjective: `lockoutActive_`, `lockedOut_`
- Verb: `preservePosition_`, `showMessages_`

## Recommended Fix
Adopt a consistent convention for boolean member variables:

**Recommended: Use adjective or past participle forms consistently**
```cpp
// State booleans (describe current state):
bool isInitialized_ = false;      // or just: initialized_ = false;
bool isPinLoaded_ = false;        // or: pinLoaded_ = false;
bool isActive_ = false;           // or: active_ = false;

// Flag booleans (describe configuration/state):
bool hasSlotRange_ = false;
bool preservePosition_ = false;   // OK - implies "should preserve"
bool showMessages_ = true;        // OK - implies "should show"
```

**Key rules:**
1. State variables: Use past participle (`initialized_`, `started_`, `stopped_`)
2. Configuration flags: Use verb phrase (`showMessages_`, `preservePosition_`)
3. Avoid `is` prefix for members (use for methods: `isLockedOut()`)

**Steps:**
1. Audit all boolean member variables in the codebase
2. Document the chosen convention in a style guide
3. Fix inconsistent names in `PinManager`, `ListView`, `PinEntryView`, and other classes
4. Ensure getter methods use consistent `isXxx()`, `hasXxx()`, `areXxx()` prefixes

## References
- [Google C++ Style Guide - Boolean Variables](https://google.github.io/styleguide/cppguide.html#Boolean_Variables)
- [C++ Core Guidelines - Naming](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rc-naming)

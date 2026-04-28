---
title: "[LOW] Feature flags create implicit coupling in command handlers"
severity: LOW
domain: architecture/coupling
lens: coupling-analysis
labels:
  - "feature-flags"
  - "compile-time-coupling"
---

## Summary
The `SerialCmd` component uses `#if FEATURE_SECURE_SERIAL` to conditionally compile authentication commands, creating implicit coupling between command handlers and feature flags. This makes it harder to add/remove features without modifying the command processor.

**Evidence:**
- `components/serial_cmd/src/SerialCmd.cpp:811-861`: `cmdAuth()` and `cmdLogout()` wrapped in `#if FEATURE_SECURE_SERIAL`
- `components/serial_cmd/src/SerialCmd.cpp:1200-1206`: `getCommandRegistry().setAuthProvider()` conditional
- `components/serial_cmd/src/SerialCmd.cpp:1346-1363`: `isAuthenticated()` conditional
- `components/serial_cmd/include/cdc_core/feature_flags.h:16-22`: Feature flag definition

## Impact
1. **Code duplication**: Similar command registration patterns for enabled/disabled features
2. **Hard to extend**: Adding new feature flags requires more conditional compilation
3. **Testing complexity**: Need to compile with different flags for feature testing
4. **Maintenance burden**: Feature flags scattered across codebase

## Evidence
File: `components/serial_cmd/src/SerialCmd.cpp`
```cpp
// Lines 811-861: Conditional command registration
#if FEATURE_SECURE_SERIAL
static void cmdAuth(const char* args) {
    // ...
}
static void cmdLogout(const char* args) {
    // ...
}
#endif

// Lines 1200-1206: Conditional provider setup
void SerialCmd::init() {
    // ...
#if FEATURE_SECURE_SERIAL
    getCommandRegistry().setAuthProvider(isAuthenticated);
    getCommandRegistry().setOnCommandExecuted(resetAuthTimer);
#endif
    // ...
}
```

File: `components/cdc_core/include/cdc_core/feature_flags.h`
```cpp
// Lines 16-22: Feature flag definition
#ifdef CONFIG_SECURE_SERIAL
#define FEATURE_SECURE_SERIAL 1
#else
#define FEATURE_SECURE_SERIAL 0
#endif
```

## Recommended Fix
1. **Use runtime feature flags**:
   ```cpp
   class FeatureFlags {
   public:
       bool isSecureSerialEnabled() const { return enabled_; }
   };
   ```

2. **Register commands with feature check**:
   ```cpp
   void SerialCmd::registerBuiltinCommands() {
       auto& reg = getCommandRegistry();
       
       reg.registerCommand({"HELP", ...}, always: true);
       
       #if FEATURE_SECURE_SERIAL
       reg.registerCommand({"AUTH", ...}, requiresAuth: true);
       #endif
   }
   ```

3. **Or use command attributes**:
   ```cpp
   struct CommandInfo {
       const char* name;
       const char* help;
       void(*handler)(const char*);
       const char* category;
       uint8_t requiredFlags;  // Bitmask of feature flags
   };
   ```

## References
- Feature flags: `components/cdc_core/include/cdc_core/feature_flags.h`
- Command registration: `components/serial_cmd/src/SerialCmd.cpp:1435-1489`
- Command registry: `components/serial_cmd/include/serial_cmd/ICommandRegistry.h`

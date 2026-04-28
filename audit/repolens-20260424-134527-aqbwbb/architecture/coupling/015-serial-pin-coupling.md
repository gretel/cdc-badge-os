---
title: "[MEDIUM] Serial command processor couples to PinManager singleton"
severity: MEDIUM
domain: architecture/coupling
lens: coupling-analysis
labels:
  - "command-coupling"
  - "singleton-dependency"
---

## Summary
The `SerialCmd` component directly accesses `PinManager::instance()` for authentication, creating tight coupling between the command processor and security state. This makes it hard to test commands independently or swap authentication mechanisms.

**Evidence:**
- `components/serial_cmd/src/SerialCmd.cpp:811-843`: `cmdAuth()` calls `PinManager::instance()` directly
- `components/serial_cmd/src/SerialCmd.cpp:1366-1389`: `authenticate()` calls `PinManager::instance().verifyBadgePin()`
- No dependency injection or abstraction for PIN verification

## Impact
1. **Testing difficulty**: Cannot test commands without initializing PinManager singleton
2. **Hard to swap auth**: Cannot easily use different authentication (e.g., for testing)
3. **Hidden dependency**: Command processor depends on security module not visible in interface
4. **Tight coupling**: Changes to PinManager affect command processor

## Evidence
File: `components/serial_cmd/src/SerialCmd.cpp`
```cpp
// Lines 811-843: cmdAuth() directly accesses PinManager singleton
static void cmdAuth(const char* args) {
    auto& pm = core::PinManager::instance();  // Direct singleton access
    
    if (pm.isBadgeBlocked()) {
        // ...
    }
    
    if (!pm.verifyBadgePin(pin)) {
        // ...
    }
}

// Lines 1366-1389: authenticate() also uses singleton
bool SerialCmd::authenticate(const char* pin) {
    auto& pm = core::PinManager::instance();  // Direct singleton access
    
    if (!pm.verifyBadgePin(pin)) {
        // ...
    }
}
```

## Recommended Fix
1. **Inject PIN manager interface**:
   ```cpp
   class IPinManager {
   public:
       virtual bool verifyBadgePin(const char* pin) = 0;
       virtual bool isBadgeBlocked() = 0;
       // ...
   };
   
   class SerialCmd {
   public:
       void setPinManager(IPinManager* pm);
   };
   ```

2. **Or use service registry**:
   ```cpp
   // Register PinManager as service
   ServiceRegistry::instance().provide<IPinManager>(
       ServiceType::PIN_MANAGER, &pinManager);
   
   // In SerialCmd
   auto* pm = ServiceRegistry::instance().request<IPinManager>(
       ServiceType::PIN_MANAGER);
   ```

3. **Provide default implementation**:
   ```cpp
   class SerialCmd {
   public:
       void setAuthProvider(std::function<bool(const char*)> fn);
   private:
       std::function<bool(const char*)> authProvider_;
   };
   ```

## References
- SerialCmd implementation: `components/serial_cmd/src/SerialCmd.cpp:811-843`
- PinManager singleton: `components/cdc_core/include/cdc_core/PinManager.h`
- ServiceRegistry: `components/cdc_core/include/cdc_core/ServiceRegistry.h`

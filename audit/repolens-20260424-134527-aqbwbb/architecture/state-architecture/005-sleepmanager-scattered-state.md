---
title: "[MEDIUM] SleepManager static variables scattered across multiple files"
severity: MEDIUM
domain: State Management Architecture
lens: state-architecture
labels:
  - "audit:architecture/state-architecture"
---

## Summary
The sleep management system uses static variables scattered across `SleepManager.h` and `AppUiInternal.h` instead of being encapsulated in the `SleepManager` singleton. This creates implicit global state that can be modified from multiple places.

### Evidence:
- `SleepManager.h:27-28`: Static constants `LIGHT_SLEEP_TIMEOUT_MS`, `MAX_SLEEP_INHIBITORS`
- `AppUiInternal.h:27-29`: Static constants `TOAST_DURATION_SHORT_MS`, `TOAST_DURATION_MEDIUM_MS`, `TOAST_DURATION_LONG_MS`
- `SleepManager.h:69-75`: Instance variables `inhibitors_[MAX_SLEEP_INHIBITORS]`, `inhibitorCount_`
- `AppUiInternal.h:45-46`: Functions `rebuildMainMenu()`, `rebuildToolsMenu()` modify global menu state

### State Access Pattern:
```cpp
// Any module can add sleep inhibitor
SleepManager::instance().addSleepInhibitor("module_name");

// But menu state is modified via AppUiInternal.h functions
rebuildMainMenu();  // Called from multiple places
```

## Impact
1. **Inconsistent sleep state**: Multiple modules can add inhibitors without coordination
2. **Menu state mutation**: `rebuildMainMenu()` is called from multiple files (`WifiMenuUi.cpp`, `BluetoothMenuUi.cpp`, `ExpertMenuUi.cpp`) without clear ownership
3. **No sleep state query**: No API to check current sleep state or list active inhibitors
4. **Static constants**: Toast durations are hardcoded in `AppUiInternal.h` instead of being configurable

## Recommended Fix
1. Add sleep state query API:
   ```cpp
   typedef struct {
       bool isInLightSleep;
       uint8_t inhibitorCount;
       const char* inhibitors[MAX_SLEEP_INHIBITORS];
       uint32_t lockScreenEnteredMs;
   } SleepManagerState;
   
   SleepManagerState getState() const;
   ```

2. Add inhibitor tracking:
   ```cpp
   bool addSleepInhibitor(const char* reason, uint32_t timeoutMs = 0);
   // Returns true if added, false if already exists
   
   void removeSleepInhibitor(const char* reason);
   ```

3. Move menu rebuild to event-driven:
   ```cpp
   // Instead of calling rebuildMainMenu() directly
   EventBus::instance().publish(EventType::MENU_REBUILD);
   
   // Menu views subscribe to MENU_REBUILD events
   ```

4. Make toast durations configurable:
   ```cpp
   void setToastDurations(uint32_t shortMs, uint32_t mediumMs, uint32_t longMs);
   ```

## References
- `components/cdc_os_ui/include/cdc_os_ui/SleepManager.h` - Sleep manager header
- `components/cdc_os_ui/src/AppUiInternal.h` - Shared constants and functions
- `components/cdc_os_ui/src/WifiMenuUi.cpp` - Menu rebuild usage
- `components/cdc_os_ui/src/BluetoothMenuUi.cpp` - Menu rebuild usage

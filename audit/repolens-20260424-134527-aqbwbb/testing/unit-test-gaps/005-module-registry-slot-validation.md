---
title: "[HIGH] ModuleRegistry Slot Validation Lacks Unit Test Coverage"
severity: HIGH
domain: Testing
lens: unit-test-gaps
labels:
  - "audit:testing/unit-test-gaps"
---

## Summary
The `ModuleRegistry` class (`components/cdc_core/src/ModuleRegistry.cpp`, 914 lines) manages module lifecycle and secure element slot allocation with no dedicated unit tests. Critical untested functions include:

- `registerModule()` (line 67) - Module registration with slot validation
- `applySlotRequest()` (line 872) - Slot request validation and assignment
- `validateEccRange()` (line 789) - ECC slot range validation
- `validateRmemRange()` (line 817) - RMEM slot range validation
- `validateSlotMap()` (line 770) - Global slot map validation
- `reportModuleError()` (line 671) - Error state with event publishing
- `retryModule()` (line 715) - Module recovery
- `setModuleEnabled()` (line 551) - NVS persistence
- `cleanupOrphanedModuleData()` (line 370) - NVS garbage collection
- `startModule()` (line 175) - Single module start with error checks

## Impact
**Module System Risk:** ModuleRegistry controls all module lifecycle and secure element access:
1. Slot validation (ECC/RMEM range, module ID mismatch) is untested
2. `applySlotRequest()` has 5-step orchestration with multiple failure paths
3. `reportModuleError()` stops module and publishes event - untested
4. `cleanupOrphanedModuleData()` erases NVS - critical for data integrity
5. `setModuleEnabled()` string parsing (comma-separated list) has edge cases

## Evidence
File: `components/cdc_core/src/ModuleRegistry.cpp`

Line 872-914: `applySlotRequest()` - Complex 5-step validation
```cpp
// Step 1: Validate slot map is initialized
if (!validateSlotMap(moduleName)) {
    return false;
}

// Step 2: Get slot request from module
IModule::SlotRequest req = module->getSlotRequest();
const char* mapName = (req.mapName && req.mapName[0] != '\0') ? req.mapName : moduleName;

// No slots required
if (!mapName || (req.minEccSlots == 0 && req.minRmemSlots == 0)) {
    return true;
}

// Step 3: Validate ECC slot range
if (req.minEccSlots > 0) {
    if (!validateEccRange(mapName, moduleName, req.minEccSlots, range, moduleId)) {
        return false;
    }
}

// Step 4: Validate RMEM slot range
if (req.minRmemSlots > 0) {
    if (!validateRmemRange(mapName, moduleName, req.minRmemSlots, range, moduleId)) {
        return false;
    }
}

// Step 5: Apply validated range
range.moduleId = moduleId;
module->setSlotRange(range);
return true;
```

Line 671-696: `reportModuleError()` - Error handling with event
```cpp
void ModuleRegistry::reportModuleError(const char* name, const char* message) {
    for (uint8_t i = 0; i < count_; i++) {
        if (strcmp(modules_[i]->getName(), name) == 0) {
            if (modules_[i]->getState() == ServiceState::STARTED) {
                modules_[i]->stop();
                LOG_W(TAG, "Module '%s' stopped due to error", name);
            }
            setModuleError(i, message);
            // Publish event for UI
            Event evt;
            evt.type = EventType::MODULE_ERROR;
            evt.data.value = i;
            EventBus::instance().publish(evt);
            return;
        }
    }
}
```

Line 370-425: `cleanupOrphanedModuleData()` - NVS erasure
```cpp
char* token = strtok_r(savedList, ",", &saveptr);
while (token) {
    bool found = false;
    for (uint8_t i = 0; i < count_; i++) {
        if (strcmp(modules_[i]->getName(), token) == 0) {
            found = true;
            break;
        }
    }
    if (!found) {
        char nsName[20];
        snprintf(nsName, sizeof(nsName), "%s%s", NVS_PREFIX, token);
        nvs_handle_t modHandle;
        if (nvs_open(nsName, NVS_READWRITE, &modHandle) == ESP_OK) {
            nvs_erase_all(modHandle);  // Erases all module data
            nvs_commit(modHandle);
            nvs_close(modHandle);
        }
    }
    token = strtok_r(nullptr, ",", &saveptr);
}
```

Line 551-602: `setModuleEnabled()` - String parsing
```cpp
if (enabled) {
    // Remove name from disabled list
    char newList[MAX_DISABLED_LIST_SIZE] = {0};
    size_t newOffset = 0;
    const char* ptr = disabledModules_;
    while (*ptr) {
        while (*ptr == ',') ptr++;  // Skip leading commas
        const char* end = ptr;
        while (*end && *end != ',') end++;
        size_t tokenLen = end - ptr;
        if (!(tokenLen == nameLen && strncmp(ptr, name, nameLen) == 0)) {
            // Copy token
        }
        ptr = end;
    }
} else {
    // Add name to disabled list
}
```

Current test coverage:
```bash
$ find test/ -name "*.cpp" -exec grep -l "ModuleRegistry" {} \;
# Returns nothing - no ModuleRegistry tests exist
```

## Recommended Fix
Create `test/test_module_registry/test_module_registry.cpp` with test cases:

1. **Registration tests:**
   - Test `registerModule()` with valid slot request
   - Test `registerModule()` with missing ECC range
   - Test `registerModule()` with RMEM/ECC module ID mismatch

2. **Error handling tests:**
   - Test `reportModuleError()` stops module and publishes event
   - Test `hasModuleSlotError()` returns true after error
   - Test `retryModule()` clears error and restarts

3. **Enable/disable tests:**
   - Test `setModuleEnabled()` persists to NVS
   - Test `isModuleEnabledByName()` parses comma list
   - Test empty list = all enabled

4. **Cleanup tests:**
   - Test `cleanupOrphanedModuleData()` erases removed module NVS
   - Test `cleanupOrphanedModuleData()` keeps existing module NVS

Example test:
```cpp
void test_reportModuleError_stops_and_publishes() {
    auto& reg = ModuleRegistry::instance();
    auto* mod = new MockModule("test");
    mod->start();  // Set to STARTED
    reg.registerModule(mod);
    
    reg.reportModuleError("test", "Test error");
    
    TEST_ASSERT_TRUE(reg.hasModuleSlotError(0));
    TEST_ASSERT_EQUAL_STRING("Test error", reg.getModuleSlotError(0));
    TEST_ASSERT_EQUAL(ServiceState::STOPPED, mod->getState());
}

void test_setModuleEnabled_persists() {
    auto& reg = ModuleRegistry::instance();
    auto* mod = new MockModule("test");
    reg.registerModule(mod);
    
    reg.setModuleEnabled(0, false);
    TEST_ASSERT_FALSE(reg.isModuleEnabled(0));
    
    // Simulate reload
    reg.loadDisabledList();
    TEST_ASSERT_FALSE(reg.isModuleEnabled(0));
}

void test_validateEccRange_insufficient_slots() {
    auto& reg = ModuleRegistry::instance();
    // Setup slot map with only 2 ECC slots
    IModule::SlotRange range = {};
    uint8_t moduleId = 0;
    bool result = reg.validateEccRange("test", "test", 5, range, moduleId);  // Need 5
    TEST_ASSERT_FALSE(result);  // Only have 2
}
```

## References
- File: `components/cdc_core/include/cdc_core/ModuleRegistry.h` - Full API
- File: `components/cdc_core/include/cdc_core/IModule.h` - Module interface
- File: `components/cdc_core/include/cdc_core/TropicSlotMap.h` - Slot mapping

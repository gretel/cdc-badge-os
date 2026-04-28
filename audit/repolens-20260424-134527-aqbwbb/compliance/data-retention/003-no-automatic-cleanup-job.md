---
title: "[LOW] No Automatic Data Cleanup or Archival Job"
severity: LOW
domain: compliance
lens: data-retention
labels:
  - "audit:compliance/data-retention"
---

## Summary
There is no scheduled task, background job, or boot-time check that automatically identifies and removes expired or stale data. Cleanup is purely manual (user-initiated delete commands).

**Location**: No cron, FreeRTOS timer, or periodic task exists for data cleanup. `components/cdc_core/src/ModuleRegistry.cpp:364` has `cleanupOrphanedModuleData()` but this only handles module removal, not data expiration.

## Impact
1. **Manual Burden**: Users must remember to clean up old data
2. **Storage Exhaustion**: Eventually TROPIC01 slots may fill up
3. **Orphaned Sessions**: Session keys and temporary data persist in memory
4. **No Self-Service**: No "cleanup old data" feature for users

## Evidence
**Module cleanup exists but is narrow scope** (`ModuleRegistry.cpp:364-420`):
```cpp
void ModuleRegistry::cleanupOrphanedModuleData() {
    // Only removes NVS data for modules no longer in firmware
    // Does NOT handle expiration of user data
}
```

**No periodic cleanup timer** - searching for FreeRTOS timers or scheduled tasks:
- No `xTimerCreate()` calls for cleanup
- No `vTaskDelay()` based background jobs
- No periodic tick handler that checks for expired data

**Manual deletion commands only** (`docs/SERIAL_COMMANDS.md`):
- `TOTP_DEL <index>` - Manual delete
- `PASSWORD_DEL <index>` - Manual delete
- `TR01_RMEM_DEL <slot>` - Manual delete
- No `CLEANUP_EXPIRED` or similar command

## Recommended Fix
Implement a simple periodic cleanup check:

**Option 1: Boot-time check** (simplest)
Add to `ModuleRegistry::runAllInitializers()`:
```cpp
void ModuleRegistry::runAllInitializers() {
    // ... existing initializers ...
    
    // Check for expired data
    checkExpiredData();
}

static void checkExpiredData() {
    // Scan FIDO2, TOTP, Password stores for expired entries
    // Log count of expired items (auto-delete optional)
}
```

**Option 2: Background task** (more proactive)
Create a low-priority FreeRTOS task that runs every 24 hours:
```cpp
void createCleanupTask() {
    xTimerCreate("cleanup", pdMS_TO_TICKS(24 * 3600 * 1000),
                 pdTRUE, NULL, cleanupTaskFn);
}
```

**Option 3: On-demand command**
Add `CLEANUP_EXPIRED` serial command that:
- Lists expired entries
- Prompts for confirmation
- Deletes them in batch

## References
- FreeRTOS timer documentation
- ESP-IDF timer API (`esp_timer`)

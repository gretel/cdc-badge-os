---
title: "[LOW] Secure element session not released on error paths"
severity: LOW
domain: connection-mgmt
lens: performance/connection-mgmt
labels:
  - "audit:performance/connection-mgmt"
---

## Summary
In `components/cdc_core/src/TropicStorage.cpp`, the `rebuildVerbose()` function starts a secure element session if one is not active, but does not track whether it started a new session or used an existing one. This means if the function returns early due to an error, a session that was started by `rebuildVerbose()` itself might remain active longer than necessary.

**Specific location:**
- `rebuildVerbose()` (line 211-275): Starts session at line 219-224, but no tracking of session ownership

Additionally, in `main/main.cpp` (line 169-175), the secure element session is started during boot and kept active, but there is no mechanism to release it during idle periods or before sleep.

## Impact
1. **Power consumption**: The TROPIC01 secure element stays in active mode with session established, consuming more power than sleep mode. The chip has an auto-sleep feature (enabled at line 218-225 in Tropic01Element.cpp), but an active session prevents deep sleep.

2. **Resource contention**: While the session is active, other operations that need the secure element share the same session. If an error leaves the session in a stale state, subsequent operations might fail.

3. **Session timeout**: If the session remains active too long without activity, the chip might timeout the session anyway, requiring re-establishment.

## Evidence
**File: `components/cdc_core/src/TropicStorage.cpp`**

1. **rebuildVerbose() - session started without tracking ownership:**
```cpp
bool TropicStorage::rebuildVerbose(RebuildLogFn logFn, void* ctx) {
    if (!secureElement_) {
        LOG_E(TAG, "No secure element set");
        return false;
    }
    if (!secureElement_->isSessionActive()) {
        if (!secureElement_->sessionStart()) {  // Session started here
            if (logFn) logFn(0xFFFF, "session start failed", ctx);
            return false;  // Session not tracked - caller doesn't know it was started
        }
    }
    // ... processing loop ...
    // If error occurs here, session might have been started by this function
    // but there's no way to know to release it
    for (uint16_t chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++) {
        // ...
        auto res = secureElement_->rmemReadWithHeader(slot, &header, nullptr, 0, &payloadLen);
        if (res == cdc::hal::SeResult::OK) {
            // ...
        }
        // ...
        if (!saveChunk(chunkIndex, chunk)) {
            if (logFn) logFn(slotBase, "nvs write failed", ctx);
            return false;  // Early return - session ownership unclear
        }
    }
    // ...
}
```

**File: `main/main.cpp`**

2. **Boot sequence - session started and never released:**
```cpp
// === SECURE ELEMENT INITIALIZATION ===
s_secureElement = cdc::hal::getSecureElementInstance();
if (s_secureElement && s_secureElement->init() && s_secureElement->start()) {
    if (s_secureElement->sessionStart()) {  // Session started
        LOG_I(TAG, "Secure Element ready (TROPIC01, session active)");
    } else {
        LOG_W(TAG, "Secure Element initialized but session start failed");
    }
}
// ...
// Session remains active throughout system runtime
// No mechanism to release before sleep or during idle
```

**File: `components/cdc_hal/src/Tropic01Element.cpp`**

3. **Auto-sleep enabled but session keeps chip awake:**
```cpp
bool Tropic01Element::sessionStart() {
    // ...
    sessionActive_ = true;
    eccCacheValid_ = false;

    // Enable chip auto-sleep mode
    uint32_t sleepCfg = 0;
    if (lt_r_config_read(&handle_, TR01_CFG_SLEEP_MODE_ADDR, &sleepCfg) == LT_OK) {
        if (!(sleepCfg & 0x01)) {
            sleepCfg |= 0x01;
            if (lt_r_config_write(&handle_, TR01_CFG_SLEEP_MODE_ADDR, sleepCfg) == LT_OK) {
                LOG_I(TAG, "Auto-sleep enabled");
            }
        }
    }
    // ...
}
```

## Recommended Fix
Track session ownership so that the function that starts a session is responsible for ending it:

1. **Add session ownership tracking to rebuildVerbose():**
```cpp
bool TropicStorage::rebuildVerbose(RebuildLogFn logFn, void* ctx) {
    if (!secureElement_) {
        LOG_E(TAG, "No secure element set");
        return false;
    }
    
    bool sessionStartedHere = false;
    if (!secureElement_->isSessionActive()) {
        if (!secureElement_->sessionStart()) {
            if (logFn) logFn(0xFFFF, "session start failed", ctx);
            return false;
        }
        sessionStartedHere = true;
    }
    
    CacheEntry chunk[CHUNK_SLOTS] = {};
    uint16_t totalChunks = ...;
    
    for (uint16_t chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++) {
        // ... processing ...
        if (!saveChunk(chunkIndex, chunk)) {
            if (logFn) logFn(slotBase, "nvs write failed", ctx);
            if (sessionStartedHere) {
                secureElement_->sessionEnd();
            }
            return false;
        }
    }
    
    cacheValid_ = saveHeader();
    
    // Release session if we started it
    if (sessionStartedHere && cacheValid_) {
        secureElement_->sessionEnd();
    }
    
    return cacheValid_;
}
```

2. **Add session management before sleep in main.cpp:**
```cpp
// In main loop, before entering sleep:
if (s_sleepController && s_sleepController->shouldEnterLightSleep()) {
    // Release secure element session before sleep
    if (s_secureElement && s_secureElement->isSessionActive()) {
        s_secureElement->sleep();  // Ends session and puts chip to sleep
    }
}
```

3. **Alternatively, use RAII-style session guard:**
```cpp
// Add to TropicStorage.h:
class SessionGuard {
public:
    SessionGuard(cdc::hal::ISecureElement* se, bool& startedHere)
        : se_(se), startedHere_(true) {
        if (se_ && !se_->isSessionActive()) {
            if (se_->sessionStart()) {
                startedHere_ = true;
            } else {
                startedHere_ = false;
            }
        } else {
            startedHere_ = false;
        }
    }
    
    ~SessionGuard() {
        if (startedHere_ && se_) {
            se_->sessionEnd();
        }
    }
    
private:
    cdc::hal::ISecureElement* se_;
    bool& startedHere_;
};

// Usage in rebuildVerbose():
bool TropicStorage::rebuildVerbose(RebuildLogFn logFn, void* ctx) {
    bool startedHere = false;
    SessionGuard guard(secureElement_, startedHere);
    
    if (!guard.isValid()) {
        LOG_E(TAG, "No secure element or session failed");
        return false;
    }
    // ... rest of function ...
    // Session automatically released when guard goes out of scope
}
```

## References
- [TROPIC01 Sleep Modes](https://www.microchip.com/en-us/products/security-ics/tropic01) - Chip power management
- [ESP-IDF Power Management](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/power_management.html) - System sleep patterns
- [RAII Pattern](https://en.wikipedia.org/wiki/Resource_acquisition_is_initialization) - Resource management pattern

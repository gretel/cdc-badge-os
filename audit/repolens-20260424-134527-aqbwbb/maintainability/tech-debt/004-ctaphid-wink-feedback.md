---
title: "[LOW] CTAPHID WINK command lacks visual feedback"
severity: LOW
domain: maintainability
lens: tech-debt/features
labels:
  - fido2
  - ux
---

## Summary
The CTAPHID WINK command handler has a TODO comment indicating missing visual feedback implementation. Located in `components/mod_fido2/src/ctaphid.cpp` line 268.

## Impact
- **UX inconsistency**: WINK command should provide user feedback but doesn't
- **Debugging difficulty**: Hard to verify WINK is working without visual indication
- **FIDO2 spec compliance**: WINK is meant for device identification, visual feedback improves usability

## Evidence
File: `components/mod_fido2/src/ctaphid.cpp`
Lines 267-270:
```cpp
static void handle_wink(uint32_t cid) {
    // TODO: Visual feedback (LED blink or similar)
    prepare_response(cid, CTAPHID_WINK, NULL, 0);
    if (CTAPHID_DEBUG_PACKETS) LOG_D("CTAPHID", "WINK");
}
```

## Recommended Fix

Add LED blink feedback using existing PinManager or GPIO:

```cpp
static void handle_wink(uint32_tcid) {
    // Visual feedback: blink LED
    #ifdef LED_BUILTIN
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
    #endif
    
    prepare_response(cid, CTAPHID_WINK, NULL, 0);
    if (CTAPHID_DEBUG_PACKETS) LOG_D("CTAPHID", "WINK");
}
```

Or use the project's PinManager if available:
```cpp
static void handle_wink(uint32_t cid) {
    auto* pinMgr = cdc::core::PinManager::getInstance();
    if (pinMgr) {
        pinMgr->blinkPin(cdc::core::PinType::LED, 100);
    }
    
    prepare_response(cid, CTAPHID_WINK, NULL, 0);
    if (CTAPHID_DEBUG_PACKETS) LOG_D("CTAPHID", "WINK");
}
```

## References
- FIDO2 CTAPHID spec: https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#command-wink

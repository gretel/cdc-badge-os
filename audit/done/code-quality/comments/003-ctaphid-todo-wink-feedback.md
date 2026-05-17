---
title: "[LOW] TODO comment in FIDO2 CTAPHID WINK handler lacks implementation detail"
severity: LOW
domain: code-quality/comments
lens: comments
labels:
  - "audit:code-quality/comments"
---

## Summary
A TODO comment in the FIDO2 module's CTAPHID transport layer indicates missing visual feedback for the WINK command:

**File**: `components/mod_fido2/src/ctaphid.cpp:267`  
**Line**: 267  
**Comment**: `// TODO: Visual feedback (LED blink or similar)`

The WINK command in CTAPHID (USB HID transport for FIDO2) is used to help users identify which security key to use when multiple devices are connected. Currently, the implementation just sends an empty response without any visual indicator.

## Impact
- **UX impact**: Users may have difficulty identifying the correct badge when multiple USB keys are connected
- **CTAPHID spec compliance**: The WINK command is designed specifically for visual/audible identification
- **Minor**: FIDO2 functionality still works, just less user-friendly

## Evidence
**File**: `components/mod_fido2/src/ctaphid.cpp`  
**Lines 264-270**:
```cpp
/**
 * \brief Handles CTAPHID WINK requests.
 * \param cid Request channel identifier.
 */
static void handle_wink(uint32_t cid) {
    // TODO: Visual feedback (LED blink or similar)
    prepare_response(cid, CTAPHID_WINK, NULL, 0);
    if (CTAPHID_DEBUG_PACKETS) LOG_D("CTAPHID", "WINK");
}
```

## Recommended Fix
Implement visual feedback using the badge's LED or display:

**Option 1 - LED blink** (if hardware supports):
```cpp
static void handle_wink(uint32_t cid) {
    // Blink LED 3 times
    for (int i = 0; i < 3; i++) {
        // Toggle LED (implement based on hardware)
        // gpio_set_level(LED_PIN, 1);
        // vTaskDelay(pdMS_TO_TICKS(200));
        // gpio_set_level(LED_PIN, 0);
        // vTaskDelay(pdMS_TO_TICKS(200));
    }
    prepare_response(cid, CTAPHID_WINK, NULL, 0);
}
```

**Option 2 - Display flash**:
```cpp
static void handle_wink(uint32_t cid) {
    // Flash display with "WINK" or icon
    auto* display = CDC_SERVICE(IDisplay, "display");
    if (display) {
        display->fillRect(0, 0, display->getWidth(), display->getHeight(), WHITE);
        display->setCursor(20, 30);
        display->print("WINK!");
        display->flushSync();
        vTaskDelay(pdMS_TO_TICKS(1000));
        display->clear();
    }
    prepare_response(cid, CTAPHID_WINK, NULL, 0);
}
```

## References
- FIDO2 CTAPHID spec: https://fidoalliance.org/specs/fido-v2.0-rd-20180130/fido-client-to-authenticator-protocol-v2.0-rd-20180130.html#wink-command
- CTAPHID_WINK: "CA uses this command to cause the authenticator to begin a wink. The authenticator should wink (e.g. blink an LED) to allow the CA to identify the authenticator."

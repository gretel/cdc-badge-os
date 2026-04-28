---
title: "[LOW] UsbManager.h struct definitions missing documentation"
severity: LOW
domain: documentation/code-docs
lens: code-docs
labels:
  - "audit:documentation/code-docs"
---

## Summary

The `UsbManager.h` file has two struct definitions (`UsbHidCallbacks` and `UsbInterfaceSpec`) without documentation explaining their purpose and field meanings.

**File:** `components/cdc_core/include/cdc_core/UsbManager.h:19-43`

## Impact

- Unclear how to use these structs for USB HID configuration
- Callback parameters not explained

## Evidence

```cpp
// Line 19-43: Undocumented structs
struct UsbHidCallbacks {
    void (*onConnect)();
    void (*onDisconnect)();
    bool (*onRead)(uint8_t* buffer, uint16_t len);
    bool (*onWrite)(const uint8_t* buffer, uint16_t len);
    bool (*isReady)();
};

struct UsbInterfaceSpec {
    uint8_t reportId;
    uint8_t interfaceNum;
    uint8_t reportCount;
    uint8_t reportSize;
    bool (*isAvailable)();
    bool (*isReady)();
    void (*onConnect)();
    void (*onDisconnect)();
    bool (*onRead)(uint8_t* buffer, uint16_t len);
    bool (*onWrite)(const uint8_t* buffer, uint16_t len);
    const uint8_t* reportDesc;
    uint16_t reportDescLen;
};
```

## Recommended Fix

Add documentation:

```cpp
/**
 * \brief USB HID callbacks for simple interface registration.
 */
struct UsbHidCallbacks {
    void (*onConnect)();          ///< Called when USB connected
    void (*onDisconnect)();       ///< Called when USB disconnected
    bool (*onRead)(uint8_t* buffer, uint16_t len);  ///< Read from host
    bool (*onWrite)(const uint8_t* buffer, uint16_t len);  ///< Write to host
    bool (*isReady)();            ///< Check if ready to transfer
};

/**
 * \brief USB interface specification for HID descriptor.
 */
struct UsbInterfaceSpec {
    uint8_t reportId;             ///< HID report ID
    uint8_t interfaceNum;         ///< USB interface number
    uint8_t reportCount;          ///< Number of report entries
    uint8_t reportSize;           ///< Size of each report entry
    bool (*isAvailable)();        ///< Check if interface available
    bool (*isReady)();            ///< Check if ready for transfers
    void (*onConnect)();          ///< Connect callback
    void (*onDisconnect)();       ///< Disconnect callback
    bool (*onRead)(uint8_t* buffer, uint16_t len);  ///< Read callback
    bool (*onWrite)(const uint8_t* buffer, uint16_t len);  ///< Write callback
    const uint8_t* reportDesc;    ///< HID report descriptor
    uint16_t reportDescLen;       ///< Report descriptor length
};
```

## References

- Related: `UsbManager::addHidInterface()` uses this struct

---
title: "[MEDIUM] BLE vCard Protocol Lacks Version Field for Forward/Backward Compatibility"
severity: MEDIUM
domain: API Design
lens: api-versioning
labels:
  - "audit:api-design/api-versioning"
---

## Summary
The BLE vCard exchange protocol (`docs/ble_vcard_protocol.md`) has no version field in its frame format. When the protocol evolves, devices will not be able to negotiate compatibility or handle unknown versions gracefully.

**Evidence:**
- `docs/ble_vcard_protocol.md:88-98` - Frame format defines opcodes but no version field
- `components/mod_vcard/include/mod_vcard/ble_vcard.h:36-41` - Exchange state machine has no version negotiation

## Impact
- Devices with different protocol versions cannot interoperate
- No graceful degradation when connecting to newer/older firmware
- Breaking changes require manual firmware synchronization

## Evidence
Current frame format (from protocol doc):
```
Client -> Server (RX / Write)
- 0x01: WRITE_START, followed by 16-bit length (LE)
- 0x02: WRITE_CONT
- 0x03: WRITE_END

Server -> Client (Data / Indicate)
- 0x81: DATA_START, followed by 16-bit length (LE)
- 0x82: DATA_CONT
- 0x83: DATA_END
```

No version byte, no capability negotiation.

## Recommended Fix
Add version negotiation to BLE protocol:

1. Add version field to frame header:
```
WRITE_START: 0x01 + version (1 byte) + length (2 bytes LE)
DATA_START:  0x81 + version (1 byte) + length (2 bytes LE)
```

2. Add version negotiation state to exchange:
```cpp
typedef enum {
    VCARD_EXCHANGE_IDLE = 0,
    VCARD_EXCHANGE_CONNECTING,
    VCARD_EXCHANGE_VERSION_NEGOTIATION,  // NEW
    VCARD_EXCHANGE_DISCOVERING,
    // ... rest of states
} vcard_exchange_state_t;
```

3. Define protocol version constant:
```cpp
#define VCARD_PROTOCOL_VERSION 1
```

4. Handle version mismatch gracefully with error message

## References
- BLE Specification Vol 3, Part G (Service Discovery)
- Protocol Versioning: https://www.ietf.org/rfc/rfc2119.txt

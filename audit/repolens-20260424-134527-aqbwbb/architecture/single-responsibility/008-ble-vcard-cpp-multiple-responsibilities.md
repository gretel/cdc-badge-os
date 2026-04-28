---
title: "[MEDIUM] ble_vcard.cpp combines BLE advertising, GATT server, vCard exchange protocol, and peer discovery"
severity: MEDIUM
domain: mod_vcard
lens: single-responsibility
labels:
  - "audit:architecture/single-responsibility"
---

## Summary
`components/mod_vcard/src/ble_vcard.cpp` (1160 lines) handles multiple distinct responsibilities:
1. **BLE advertising** - `startAdvertising()`, `stopAdvertising()`, advertising data construction
2. **GATT server** - Service registration, characteristic definitions, handle management
3. **vCard exchange protocol** - State machine (`s_exchange_state`), request/response handling, consent flow
4. **Peer discovery** - Scanning, peer list management (`s_peers` array), RSSI filtering
5. **vCard formatting** - vCard generation, parsing, validation
6. **Notifications** - Status characteristic updates, consent notifications

## Impact
- **High coupling**: Changes to BLE stack, GATT protocol, or vCard format all require modifying the same file
- **Complexity**: 1160 lines with 20+ state variables makes navigation difficult
- **Testing difficulty**: Cannot test peer discovery without full BLE stack initialization
- **State management**: 20+ static state variables (`s_initialized`, `s_adv_enabled`, `s_exchange_state`, etc.)

## Evidence
File: `components/mod_vcard/src/ble_vcard.cpp`
- Lines 28-68: UUID definitions (service, characteristics)
- Lines 70-89: Protocol constants and enums
- Lines 91-180: State variables (20+ static bools, buffers, handles)
- Lines 182-400: GATT server setup and callbacks
- Lines 400-700: vCard exchange state machine
- Lines 700-900: Peer discovery and scanning
- Lines 900-1160: vCard formatting and helpers

Key state showing mixed concerns:
```cpp
// Runtime state flags (8 bools)
static bool s_initialized = false;
static bool s_adv_enabled = false;
static bool s_scan_enabled = false;
static bool s_receive_enabled = false;
static bool s_exchange_enabled = false;
static bool s_exchange_in_progress = false;
static bool s_exchange_result_ready = false;
static bool s_exchange_result = false;

// TX/RX buffers
static char s_tx_vcard[VCARD_MAX_LEN] = {};
static char s_rx_vcard[VCARD_MAX_LEN] = {};

// Peer storage
static vcard_peer_t s_peers[MAX_PEERS] = {};
static vcard_peer_t s_nearby_peer = {};

// Exchange state machine
static vcard_exchange_state_t s_exchange_state = VCARD_EXCHANGE_IDLE;
```

## Recommended Fix
Split into focused modules:
1. **BleAdv** - Advertising management in `components/mod_vcard/src/BleAdv.cpp`
2. **GattServer** - GATT service/characteristic setup in `components/mod_vcard/src/GattServer.cpp`
3. **VcardProtocol** - Exchange state machine in `components/mod_vcard/src/VcardProtocol.cpp`
4. **PeerDiscovery** - Scanning and peer list in `components/mod_vcard/src/PeerDiscovery.cpp`
5. **VcardFormat** - vCard formatting in `components/mod_vcard/src/VcardFormat.cpp`

Each module should:
- Have its own header file with specific interface
- Accept dependencies via constructor
- Be testable in isolation

Example split structure:
```
components/mod_vcard/src/
  ble_vcard.cpp     // Main orchestration, delegates to sub-modules
  BleAdv.cpp        // Advertising
  GattServer.cpp    // GATT server
  VcardProtocol.cpp // Exchange protocol
  PeerDiscovery.cpp // Scanning
  VcardFormat.cpp   // vCard formatting
```

## References
- Single Responsibility Principle: https://en.wikipedia.org/wiki/Single-responsibility_principle
- BLE GATT Specification: https://www.bluetooth.com/specifications/gatt/

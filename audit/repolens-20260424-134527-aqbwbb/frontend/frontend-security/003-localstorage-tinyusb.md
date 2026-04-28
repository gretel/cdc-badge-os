---
title: "[LOW] localStorage Usage in TinyUSB Web Serial Example"
severity: LOW
domain: frontend-security
lens: storage-security
labels:
  - "localstorage"
  - "third-party-code"
  - "tinyusb"
---

## Summary
The TinyUSB Web USB Serial example (`managed_components/espressif__tinyusb/examples/device/webusb_serial/website/application.js`) uses localStorage to store various session data including device port information, command history, and UI state.

**Location:** `/input/20260423-132359-oj8ayc/cdc-badge-os/managed_components/espressif__tinyusb/examples/device/webusb_serial/website/application.js` (multiple locations)

## Impact
localStorage stores data in the browser with no expiration. Stored data includes:
- WebUSB serial port information (line ~480)
- Command history (line ~217)
- Received data (line ~340)
- UI preferences (theme, scroll positions)

While the data stored is not highly sensitive (no authentication tokens), the patterns used here could be a reference for future implementations that might store more sensitive data.

## Evidence
Key localStorage operations found:

```javascript
// Line ~480 - Store port info
const savedPortInfo = JSON.parse(localStorage.getItem('webUSBSerialPort'));
localStorage.setItem('webUSBSerialPort', JSON.stringify(portInfo));

// Line ~217 - Store command history
let savedCommandHistory = JSON.parse(localStorage.getItem('commandHistory') || '[]');
localStorage.setItem('commandHistory', JSON.stringify(this.commandHistory));

// Line ~340 - Store received data
let savedReceivedData = JSON.parse(localStorage.getItem('receivedData') || '[]');
localStorage.setItem('receivedData', JSON.stringify(this.receivedData));

// Line ~64 - Store theme preference
const currentPreference = localStorage.getItem('theme') || 'auto';
localStorage.setItem('theme', theme);
```

## Recommended Fix
For this third-party example code, the impact is minimal since:
1. Command history and received data are not sensitive
2. Port info is just connection details, not credentials

However, for consistency and best practices:
1. Consider using sessionStorage instead of localStorage for session-only data
2. Add cleanup on disconnect to clear port info:
   ```javascript
   disconnectPort() {
     // ... existing disconnect logic ...
     localStorage.removeItem('webUSBSerialPort');
   }
   ```
3. For command history, consider limiting to non-sensitive commands only

If this pattern is used in other web interfaces, ensure:
- No authentication tokens stored in localStorage
- No PII or sensitive data stored without encryption
- Clear storage on logout/session expiry

## References
- [MDN: localStorage](https://developer.mozilla.org/en-US/docs/Web/API/Window/localStorage)
- [MDN: sessionStorage](https://developer.mozilla.org/en-US/docs/Web/API/Window/sessionStorage)
- [OWASP: Web Storage](https://cheatsheetseries.owasp.org/cheatsheets/Storage_Cheat_Sheet.html)

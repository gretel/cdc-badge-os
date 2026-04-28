---
title: "[MEDIUM] Premature WebUSB Auto-Reconnect on Page Load"
severity: MEDIUM
domain: web-flasher
lens: permission-antipatterns
labels:
  - "audit:ux-antipatterns/permission-antipatterns"
---

## Summary

The WebUSB Serial example application initiates an auto-connect attempt to WebUSB devices immediately on page load, before any user interaction. This occurs in the `Application` constructor at line 115 of `application.js`.

**File**: `/input/20260423-132359-oj8ayc/cdc-badge-os/managed_components/espressif__tinyusb/examples/device/webusb_serial/website/application.js`

**Line**: 115

## Impact

- **User Experience**: Visitors may see a WebUSB device selection dialog immediately upon page load, before they understand what the page does or have had a chance to prepare their device
- **Permission Fatigue**: Unexpected permission prompts teach users to reflexively deny permissions, potentially breaking the auto-reconnect feature for returning users
- **Confusion**: Users who didn't expect to be prompted may think something is wrong or that the page is "broken"

## Evidence

```javascript
// application.js:103-116
window.addEventListener('beforeunload', () => this.beforeUnloadHandler());

// restore state from localStorage
try {
  this.restoreState();
} catch (error) {
  console.error('Failed to restore state from localStorage', error);
  this.resetAll();
  this.restoreState();
}

this.updateUIConnectionState();
this.connectWebUsbSerialPort(true);  // Line 115 - auto-connect on page load
```

The `connectWebUsbSerialPort(true)` function (line 469-530) will:
1. Call `serial.getWebUsbSerialPorts()` to list previously connected devices
2. If no devices found and auto-reconnect is enabled, it may eventually call `serial.requestWebUsbSerialPort()` (line 500) which triggers the browser permission dialog

## Recommended Fix

Defer the auto-reconnect attempt until after the page has fully loaded and the user has had a chance to see the interface. Options include:

**Option 1: Use `setTimeout` to delay auto-reconnect**
```javascript
// After line 115, change to:
setTimeout(() => {
  this.connectWebUsbSerialPort(true);
}, 500);  // Small delay to let UI render first
```

**Option 2: Move auto-reconnect to `DOMContentLoaded` or `load` event**
```javascript
// Replace line 115 with:
window.addEventListener('DOMContentLoaded', () => {
  this.connectWebUsbSerialPort(true);
});
```

**Option 3: Make auto-reconnect truly opt-in**
Only auto-connect if the user has explicitly clicked the "Connect WebUSB" button at least once before, storing that preference in localStorage.

## References

- [WebUSB API - navigator.usb.getDevices()](https://developer.mozilla.org/en-US/docs/Web/API/USB/getDevices)
- [Web Serial API - navigator.serial.getPorts()](https://developer.mozilla.org/en-US/docs/Web/API/Serial/getPorts)
- Best practices for permission requests: [Permission Prompt Best Practices](https://developers.google.com/web/updates/2019/04/permission-prompts)

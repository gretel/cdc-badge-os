---
title: "[LOW] Auto-reconnect feature only supports WebUSB, ignores Web Serial"
severity: LOW
domain: web-flasher
lens: permission-antipatterns
labels:
  - "audit:ux-antipatterns/permission-antipatterns"
  - "webusb"
  - "web-serial"
  - "auto-reconnect"
---

## Summary

The auto-reconnect feature in `application.js` only attempts to reconnect via WebUSB, completely ignoring Web Serial as an alternative. When the user has previously connected via Web Serial (the fallback option), the auto-reconnect feature silently fails because it only checks for WebUSB devices in `localStorage` and calls `connectWebUsbSerialPort(true)` on line 115.

**File**: `/input/20260423-132359-oj8ayc/cdc-badge-os/managed_components/espressif__tinyusb/examples/device/webusb_serial/website/application.js`

**Lines**: 115, 469-535, 588-589

## Impact

- **User Experience**: Users who connected via Web Serial (on browsers that support Web Serial but not WebUSB, or who prefer Web Serial) will not benefit from auto-reconnect
- **Inconsistent Behavior**: The "Auto Reconnect WebUSB" checkbox (line 38) is misleading - it should be "Auto Reconnect" to reflect that it could support both protocols
- **Silent Failure**: When auto-reconnect is enabled but the user connected via Web Serial, nothing happens on page reload, confusing users who expect auto-reconnect to work

## Evidence

The auto-reconnect logic is triggered by the checkbox on line 588:

```javascript
// Line 588-589
uiAutoReconnectCheckbox.checked = !(localStorage.getItem('autoReconnect') === 'false');
```

On page load (line 115), only WebUSB auto-reconnect is called:

```javascript
// Line 115
this.connectWebUsbSerialPort(true);
```

The `connectWebUsbSerialPort(initial)` function (lines 469-535) only handles WebUSB devices:

```javascript
// Lines 476-491
if (initial) {
  if (!uiAutoReconnectCheckbox.checked || grantedDevices.length === 0) {
    return false;
  }

  // Connect to the device that was saved to localStorage otherwise use the first one
  const savedPortInfo = JSON.parse(localStorage.getItem('webUSBSerialPort'));
  if (savedPortInfo) {
    for (const device of grantedDevices) {
      if (device._device.vendorId === savedPortInfo.vendorId && device._device.productId === savedPortInfo.productId) {
        this.currentPort = device;
        break;
      }
    }
  }
  if (!this.currentPort) {
    this.currentPort = grantedDevices[0];
  }

  this.setStatus('Connecting to first device...', 'info');
}
```

There is no corresponding logic for Web Serial auto-reconnect. The `connectSerialPort()` function (lines 445-467) has no `initial` parameter and is only called on button click.

## Recommended Fix

Add Web Serial auto-reconnect support by:

1. **Update the checkbox label** to be more generic:

```html
<!-- In index.html, line 38 -->
<label for="auto_reconnect_checkbox" class="controls">
  <input type="checkbox" id="auto_reconnect_checkbox" />
  Auto Reconnect
</label>
```

2. **Add Web Serial port storage and retrieval**:

```javascript
// In restoreState() or a new helper function
const savedSerialPortInfo = JSON.parse(localStorage.getItem('serialPortInfo'));
if (savedSerialPortInfo) {
  // Store for potential auto-reconnect
  this.savedSerialPortInfo = savedSerialPortInfo;
}
```

3. **Modify `connectWebUsbSerialPort(initial)` to also try Web Serial**:

```javascript
async connectWebUsbSerialPort(initial = false) {
  if (!serial.isWebUsbSupported()) {
    // Fallback to Web Serial if WebUSB not supported and auto-reconnect enabled
    if (initial && uiAutoReconnectCheckbox.checked && serial.isWebSerialSupported()) {
      return this.connectSerialPort(initial);
    }
    this.setStatus('WebUSB not supported on this browser', 'error');
    return false;
  }
  // ... existing WebUSB logic ...
}
```

4. **Create a new `connectSerialPort(initial)` function** similar to `connectWebUsbSerialPort`:

```javascript
async connectSerialPort(initial = false) {
  if (!serial.isWebSerialSupported()) {
    return false;
  }
  try {
    if (initial) {
      if (!uiAutoReconnectCheckbox.checked) {
        return false;
      }
      // Retrieve previously connected serial port
      let ports = await serial.getSerialPorts();
      if (ports.length > 0) {
        this.currentPort = ports[0];
        this.setStatus('Reconnecting to Web Serial...', 'info');
      } else {
        return false;
      }
    } else {
      this.setStatus('Requesting Web Serial port...', 'info');
      this.currentPort = await serial.requestSerialPort();
    }
    // ... rest of connection logic ...
  } catch (error) {
    this.setStatus(`Web Serial connection failed: ${error.message}`, 'error');
  }
}
```

## References

- [Web Serial API - MDN](https://developer.mozilla.org/en-US/docs/Web/API/Web_Serial_API)
- [WebUSB API - MDN](https://developer.mozilla.org/en-US/docs/Web/API/WebUSB_API)
- [navigator.usb.getDevices()](https://developer.mozilla.org/en-US/docs/Web/API/USB/getDevices)
- [navigator.serial.getPorts()](https://developer.mozilla.org/en-US/docs/Web/API/Serial/getPorts)

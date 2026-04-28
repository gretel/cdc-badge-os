---
title: "[LOW] USB HID send report errors not checked in FIDO2 module"
severity: LOW
domain: error-path-tests
lens: usb-hid
labels:
  - "usb-hid"
  - "fido2"
  - "ctap"
---

## Summary
In `components/mod_fido2/src/Fido2Module.cpp` (line 282-285), the `usb_hid_send_report()` return value is not checked. This means FIDO2 assertion responses may be lost if USB HID buffer is full or USB is disconnected, with no error handling.

**Files:**
- `components/mod_fido2/src/Fido2Module.cpp:282-285`
- `components/usb_badge/usb_hid.cpp:401-420`

## Impact
1. **Lost Responses**: FIDO2 assertion data may not be sent to host
2. **Authentication Failures**: Host times out waiting for response
3. **No Retry**: Lost data isn't queued for later transmission
4. **Debug Difficulty**: FIDO2 appears to work but some authentications fail silently

## Evidence
From `components/mod_fido2/src/Fido2Module.cpp`:

```cpp
// Lines 282-285: FIDO2 assertion response - no error check
bool Fido2Module::sendAssertionResponse(const uint8_t* data, size_t len) {
    // ... prepare HID report ...
    
    bool sent = usb_hid_send_report(HID_REPORT_ID_FIDO, data, len);
    // ❌ Return value ignored in some callers
    // ❌ No retry logic
    // ❌ No queue for pending data
    
    return sent;  // Returns status but caller may not check
}

// In ctap2.cpp or u2f.cpp usage:
fido2->sendAssertionResponse(buffer, size);  // ❌ No error check!

// From components/usb_badge/usb_hid.cpp:401-420
bool usb_hid_send_report(uint8_t reportId, const uint8_t* data, size_t len) {
    // Returns true if TinyUSB accepted the report
    // Returns false if buffer full or not connected
    // ❌ No distinction between "busy" and "failed"
    return tud_hid_report(reportId, data, len) > 0;
}
```

**Problem:**
- Return value of `usb_hid_send_report()` is often ignored
- No retry logic for transient failures
- No queue for pending HID reports
- `tud_hid_report()` returns 0 if buffer full, but this is treated as "success"

## Recommended Fix
Add proper error handling for HID report transmission:

1. **Add retry logic**:
   ```cpp
   #define HID_SEND_MAX_RETRIES 5
   
   bool Fido2Module::sendAssertionResponse(const uint8_t* data, size_t len) {
       uint8_t report[HID_REPORT_SIZE];
       // ... prepare report ...
       
       for (int i = 0; i < HID_SEND_MAX_RETRIES; i++) {
           if (usb_hid_send_report(HID_REPORT_ID_FIDO, report, len)) {
               return true;
           }
           vTaskDelay(pdMS_TO_TICKS(10));  // Wait for buffer
       }
       
       LOG_E("FIDO2", "Failed to send assertion response after %d retries", 
             HID_SEND_MAX_RETRIES);
       return false;
   }
   ```

2. **Add pending report queue**:
   ```cpp
   class Fido2Module {
   private:
       StaticQueue<10, HidReport> pendingReports;
       
       bool sendOrQueue(uint8_t reportId, const uint8_t* data, size_t len) {
           if (usb_hid_send_report(reportId, data, len)) {
               return true;
           }
           
           // Queue for later
           if (pendingReports.push({reportId, data, len})) {
               // Start background sender
               startReportSender();
               return true;  // Will be sent later
           }
           
           return false;  // Queue full
       }
   };
   ```

3. **Update callers to check errors**:
   ```cpp
   // In ctap2.cpp
   if (!fido2->sendAssertionResponse(buffer, size)) {
       LOG_E("CTAP2", "Failed to send assertion, host may timeout");
       // Maybe send error response?
   }
   ```

4. **Add test cases**:
   - Mock `tud_hid_report()` to return 0 (buffer full)
   - Verify retry logic works
   - Mock persistent failure, verify queue overflow handling
   - Test successful send after retries

## References
- USB HID Specification: https://usb.org/hid
- TinyUSB HID Class: https://github.com/hathach/tinyusb/tree/master/class/hid
- FIDO2 CTAP2 HID Transport: https://fidoalliance.org/specs/fido-v2.0-rd-20180309/fido-client-to-authenticator-protocol-v2.0-rd-20180309.html#hid-transport

</content>
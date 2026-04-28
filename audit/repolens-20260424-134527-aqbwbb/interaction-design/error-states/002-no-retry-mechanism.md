---
title: "[MEDIUM] Failed data fetches show error with no retry button or automatic retry"
severity: MEDIUM
domain: interaction-design/error-states
lens: retry-mechanisms
labels:
  - "audit:interaction-design/error-states"
---

## Summary
When operations fail (WiFi connection, TOTP code generation, password entry load), the UI shows an error toast but provides no retry mechanism. Users must manually navigate back and repeat the entire process.

**Affected operations:**
- WiFi connection failures (`components/cdc_os_ui/src/WifiMenuUi.cpp`)
- NTP sync failures (`components/cdc_os_ui/src/WifiHandlers.cpp`)
- TOTP code generation (`components/mod_totp/src/TotpModule.cpp`)
- Password entry load/save (`components/mod_password/src/PasswordModule.cpp`)
- GPG key generation (`components/mod_gpg/src/GpgModule.cpp`)

## Impact
**User Frustration:** Transient failures (network timeout, temporary hardware unavailability) require full workflow restart.

**No distinction between transient and permanent failures:** All errors are treated the same - show message and stop.

## Evidence
```cpp
// components/cdc_os_ui/src/WifiMenuUi.cpp:WiFi connect
void showWifiMainMenu() {
    // ...
    auto& wifiHandlers = WifiHandlers::instance();
    bool connected = wifiHandlers.connect();
    if (!connected) {
        showToastError(tr(StringId::WIFI_FAILED), TOAST_DURATION_LONG_MS);
        // No retry button, user must navigate back and try again
        return;
    }
}

// components/mod_totp/src/TotpModule.cpp:417
void TotpCodeView::onEnter(void* context) {
    updateCode();
    if (!timeValid_) {
        ui::showToastError(mstr(STR_TIME_INVALID));  // No retry option
    }
    // User must manually re-enter view to try again
}

// components/mod_password/src/PasswordModule.cpp:576
static void wizardEdit(uint16_t slot) {
    PasswordEntry entry = {};
    if (!PasswordStore::instance().readEntry(slot, &entry)) {
        ui::showToastError(ui::tr(ui::StringId::FAILED));  // No retry
        return;
    }
}
```

## Recommended Fix
**1. Add retry callback to ToastView/MessageBox:**

Extend the toast/message API with an optional retry action:
```cpp
// ToastView.h
void showToastError(const char* message, uint16_t durationMs = 1500, 
                    std::function<void()> onRetry = nullptr);

// MessageBox.h  
void showMessage(const char* message, MessageIcon icon, uint32_t timeoutMs, 
                 MessageBox::CloseCallback onClose = nullptr,
                 MessageBox::CloseCallback onRetry = nullptr);
```

**2. Implement retry for transient failures:**

```cpp
// WiFi example
void showWifiMainMenu() {
    auto& wifiHandlers = WifiHandlers::instance();
    bool connected = wifiHandlers.connect();
    if (!connected) {
        showToastError(tr(StringId::WIFI_FAILED), TOAST_DURATION_LONG_MS, 
                       []() { showWifiMainMenu(); });  // Retry callback
    }
}

// TOTP example
void TotpCodeView::onEnter(void* context) {
    updateCode();
    if (!timeValid_) {
        ui::showToastError(mstr(STR_TIME_INVALID), 2000, 
                           []() { 
                               auto* view = ui::ViewStack::instance().current();
                               if (view && view == &s_codeView) {
                                   static_cast<TotpCodeView*>(view)->updateCode();
                               }
                           });
    }
}
```

**3. For non-critical operations, add automatic retry with backoff:**

```cpp
// NTP sync with automatic retry
bool WifiHandlers::syncNtp() {
    static int retryCount = 0;
    const int MAX_RETRIES = 3;
    
    // ... existing sync logic ...
    
    if (!connected && retryCount < MAX_RETRIES) {
        retryCount++;
        vTaskDelay(pdMS_TO_TICKS(2000 * retryCount));  // Exponential backoff
        return syncNtp();  // Recursive retry
    }
    retryCount = 0;
    return true;
}
```

**Estimated effort:** 1 hour for ToastView/MessageBox API extension, 30 min per module for implementation

## References
- [RFC 3986: Retry-After header pattern](https://tools.ietf.org/html/rfc2616#section-14.37)
- [AWS Retry Patterns](https://aws.amazon.com/builders-library/making-retries-more-than-an-exponential-backoff-algorithm/)

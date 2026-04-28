---
title: "[LOW] QRCodeView rendering integration lacks tests"
severity: LOW
domain: ui
lens: integration-test-gaps
labels:
  - "audit:testing/integration-test-gaps"
  - "component:cdc_views"
  - "area:qr-code"
---

## Summary
The `QRCodeView` component (`components/cdc_views/src/QRCodeView.cpp`) renders QR codes for TOTP secrets, but **no integration tests** verify that QR codes are generated and rendered correctly.

## Impact
- **QR generation**: QR codes may not be generated correctly
- **Rendering**: QR codes may not render correctly on display
- **Size**: QR codes may not fit display
- **Scannable**: Phones may not be able to scan the QR code

## Evidence

**QRCodeView API** (`components/cdc_views/src/QRCodeView.cpp`):
```cpp
class QRCodeView : public ViewBase {
    void init(const char* data, uint8_t size);
    void render();
};
```

**QR generation** (using `tinyqr` library):
```cpp
// Generate QR from data
TinyQr qr;
qr.encode(data, size);

// Render to display
for (int y = 0; y < qr.size; y++) {
    for (int x = 0; x < qr.size; x++) {
        if (qr.get(x, y)) {
            display->drawPixel(x, y, BLACK);
        }
    }
}
```

**Usage in TOTP module** (`components/mod_totp/src/TotpModule.cpp:600-700`):
```cpp
// Show QR code for TOTP secret
void showQrCode(const char* secret, const char* issuer, const char* account) {
    char uri[128];
    snprintf(uri, sizeof(uri), "otpauth://totp/%s:%s?secret=%s&issuer=%s",
             issuer, account, secret, issuer);
    
    QRCodeView* view = new QRCodeView();
    view->init(uri, strlen(uri));
    ViewStack::instance().push(view);
}
```

**Current test coverage**: None

## Recommended Fix

Create integration test `test_qr_code_view/` that verifies:

1. **QR generation**: QR codes generated correctly
2. **Rendering**: QR codes render to display buffer
3. **Size**: QR codes fit in display
4. **Data integrity**: QR data matches input

**Test structure** (example):
```cpp
// test/test_qr_code_view/test_qr_code.cpp
#include "cdc_views/QRCodeView.h"

void test_qr_generation() {
    const char* data = "otpauth://totp/Test:user?secret=JBSWY3DPEHPK3PXP";
    
    QRCodeView view;
    view.init(data, strlen(data));
    
    // Verify QR generated
    ASSERT_TRUE(view.isReady());
}

void test_qr_rendering() {
    const char* data = "Test";
    
    QRCodeView view;
    view.init(data, strlen(data));
    
    // Render to buffer
    uint8_t buffer[296 * 128];  // Display size
    view.render(buffer);
    
    // Verify QR pattern (simplified check)
    ASSERT_GT(buffer[0], 0);  // First pixel should be set (finder pattern)
}
```

## References
- [QRCodeView implementation](components/cdc_views/src/QRCodeView.cpp)
- [TOTP module usage](components/mod_totp/src/TotpModule.cpp:600)

</content>
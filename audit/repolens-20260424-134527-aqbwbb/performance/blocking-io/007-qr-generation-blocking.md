---
title: "[MEDIUM] Synchronous QR code generation blocking UI thread"
severity: MEDIUM
domain: performance
lens: blocking-io
labels:
  - "performance"
  - "qr-code"
  - "ui-blocking"
---

## Summary

The QR code view (`components/cdc_views/src/QRCodeView.cpp`) uses `esp_qrcode_generate()` synchronously, which blocks the calling task for 10-50ms depending on QR version and data size. This happens twice during layout calculation and rendering.

**Affected file:**
- `components/cdc_views/src/QRCodeView.cpp` (lines 130-148, 189-202)

**Evidence:**
```cpp
// components/cdc_views/src/QRCodeView.cpp:126-148
void QRCodeView::calculateLayout() {
    if (!data_) return;

    // First pass: determine actual QR size (without drawing)
    esp_qrcode_config_t cfg = {
        .display_func = qrDisplayCallback,
        .max_qrcode_version = 20,  // Max 97 modules
        .qrcode_ecc_level = ESP_QRCODE_ECC_LOW,
        .user_data = nullptr
    };

    s_qrCtx.sizingPass = true;
    s_qrCtx.actualSize = 0;
    s_qrCtx.display = nullptr;

    esp_err_t err = esp_qrcode_generate(&cfg, data_);  // Blocks 10-30ms
    if (err != ESP_OK) {
        LOG_E(TAG, "QR sizing failed: %s", esp_err_to_name(err));
        qrModuleCount_ = 97;  // Fallback
    } else {
        qrModuleCount_ = s_qrCtx.actualSize;
    }
    // ...
}

// components/cdc_views/src/QRCodeView.cpp:173-202
void QRCodeView::renderQrCode() {
    // ... setup context ...

    esp_qrcode_config_t cfg = {
        .display_func = qrDisplayCallback,
        .max_qrcode_version = 20,
        .qrcode_ecc_level = ESP_QRCODE_ECC_LOW,
        .user_data = nullptr
    };

    esp_err_t err = esp_qrcode_generate(&cfg, data_);  // Blocks 10-30ms again
    if (err != ESP_OK) {
        LOG_E(TAG, "QR render failed: %s", esp_err_to_name(err));
        // ...
    }
}
```

**Note**: `esp_qrcode_generate()` is called TWICE per QR display:
1. First call in `calculateLayout()` for sizing
2. Second call in `renderQrCode()` for actual rendering

## Impact

1. **UI freeze during QR display**: When showing a QR code (e.g., for SSH keys, TOTP secrets, FIDO2 credentials), the UI freezes for 20-60ms total (two generations).

2. **Redundant computation**: The QR code is generated twice - once for sizing and once for rendering. The QR matrix could be cached and reused.

3. **Blocking in render path**: QR generation happens synchronously during the render cycle, blocking the main task.

4. **Variable latency**: QR generation time varies based on data size and version (10ms for small, 50ms for large).

## Evidence

**QR code generation characteristics:**
- Small QR (v1-5): ~10-20ms
- Medium QR (v6-15): ~20-35ms
- Large QR (v16-20): ~35-50ms

**Called from:**
- `components/mod_gpg/src/GpgModule.cpp:212` - GPG key QR display
- `components/cdc_views/src/QRCodeView.cpp:331` - Shared QR code view

## Recommended Fix

1. **Cache QR matrix between passes** (simplest, ~30 min):
```cpp
class QRCodeView {
private:
    uint8_t qrData_[256];  // Max QR size
    size_t qrDataLen_;
    bool qrValid_;

public:
    void calculateLayout() {
        if (!qrValid_) {
            // Generate once and cache
            generateQrMatrix(data_, qrData_, &qrDataLen_);
            qrValid_ = true;
        }
        // Use cached data for sizing
        qrModuleCount_ = getQrSize(qrData_);
    }

    void renderQrCode() {
        if (!qrValid_) {
            generateQrMatrix(data_, qrData_, &qrDataLen_);
            qrValid_ = true;
        }
        // Use cached data for rendering
        renderQrMatrix(qrData_);
    }
};
```

2. **Pre-generate QR during idle time**:
```cpp
// Generate QR in background when no user interaction
void scheduleQrGeneration(const char* data) {
    static TaskHandle_t qrTask;
    // Queue data for background generation
}
```

3. **Use incremental rendering**:
```cpp
// Draw QR code in chunks, yielding between chunks
void renderQrCodeIncremental() {
    static int currentRow = 0;
    for (; currentRow < qrSize; currentRow++) {
        drawRow(currentRow);
        if (currentRow % 5 == 0) {
            yield();  // Allow other tasks
        }
    }
}
```

**Estimated effort**: 30-60 minutes for caching implementation

## References

- [ESP-QRCode component](https://components.espressif.com/components/espressif/qrcode)
- QR code generation complexity: O(n²) where n is QR version
- Typical QR sizes: 21-97 modules (versions 1-40)

</content>
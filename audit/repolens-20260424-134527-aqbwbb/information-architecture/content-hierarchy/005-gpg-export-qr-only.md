---
title: "[LOW] GPG key export uses QR code without alternative dense text view"
severity: LOW
domain: information-architecture
lens: content-hierarchy
labels:
  - "audit:information-architecture/content-hierarchy"
---

## Summary
The GPG module's public key export (`components/mod_gpg/src/GpgModule.cpp:497-509`) displays the key only in QR code format using `QRCodeView`. While QR codes are useful for scanning, they lack:
- Alternative text view for manual copying
- Section structure showing key metadata (fingerprint, date, user ID)
- Progressive disclosure for key details vs. full key material
- Clear indication of key size or format

The QR code view shows the full PEM block, which may be dense and hard to read on a small display.

## Impact
Users who want to manually copy the key (e.g., for pasting into a terminal) must decode the QR code visually or use serial output. There's no in-view option to switch between QR and text formats. Key metadata is not prominently displayed alongside the QR code.

## Evidence
File: `components/mod_gpg/src/GpgModule.cpp:497-509`
```cpp
static void showExport() {
    static char pem_buf[2048];
    size_t out_len = 0;
    if (!gpg_export_pubkey_pem(pem_buf, sizeof(pem_buf), &out_len)) {
        ui::showToastError(ui::tr(ui::tr(ui::StringId::FAILED)));
        return;
    }
    cdc::serial::Console::printf("%s\r\n", pem_buf);
    s_qrView.init(mstr(STR_EXPORT_TITLE), nullptr, pem_buf);
    ui::ViewStack::instance().push(&s_qrView);
}
```

The function outputs to serial AND shows QR code, but no in-view text alternative.

## Recommended Fix
1. Add a context menu (key '3') to toggle between QR code and text view
2. Show key metadata (fingerprint, user ID, created date) above the QR code as a summary section
3. Truncate long PEM blocks with "..." and show full text on request

Approximate effort: 1 hour to add toggle functionality and metadata display.

## References
- GPG export view: `components/mod_gpg/src/GpgModule.cpp:497-509`
- QRCodeView implementation: `components/cdc_views/src/QRCodeView.cpp`
- ContextMenuView: `components/cdc_views/include/cdc_views/ContextMenuView.h`

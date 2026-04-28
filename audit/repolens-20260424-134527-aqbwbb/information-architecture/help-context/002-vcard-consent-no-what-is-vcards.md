---
title: "[MEDIUM] vCard consent prompt lacks explanation of what vCard exchange does"
severity: MEDIUM
domain: information-architecture
lens: help-context
labels:
  - "audit:information-architecture/help-context"
---

## Summary
The vCard consent prompt (in `components/mod_vcard/src/VcardModule.cpp:130-185`) shows an "Exchange Request" but provides no explanation of what a vCard is, what information will be shared, or what the user can expect after accepting.

**Evidence** (`components/mod_vcard/src/VcardModule.cpp:130-145`):
```cpp
/**
 * \brief Handles user acceptance of incoming vCard transfer consent.
 */
static void onConsentAccept(void* userData) {
    (void)userData;
    ble_vcard_respond_consent(true);
    ui::ViewStack::instance().pop();
    ui::showToastInfo("Waiting for vCard...");
}
```

The consent view (`s_consentView`) is initialized with minimal context. Users see:
- "Exchange Request" title
- Peer device name
- "[Y] Accept  [N] Decline" hint

But no explanation of:
- What a vCard contains (name, phone, email, etc.)
- What their own vCard will contain
- How to configure their own vCard first

## Impact
Users may:
1. Accept vCard exchanges without knowing what data they're receiving
2. Accept without having configured their own vCard (leading to empty exchanges)
3. Not understand the BLE proximity-based nature of the exchange

## Evidence
- File: `components/mod_vcard/src/VcardModule.cpp`
- Lines: 100-185 (consent view setup and handlers)
- Line 229: `ui::showToastInfo("No vCard configured")` - shows vCard can be empty
- No help text or info icon explaining vCard format/content

## Recommended Fix
Add contextual help to the vCard consent flow:

1. **Enhanced consent prompt**: Include a brief description:
   ```
   Exchange Request from:
   [Peer Name]

   vCard contains:
   Name, Phone, Email
   (Your info will be shared too)

   [Y] Accept  [N] Decline  [3] Learn more
   ```

2. **Add "Learn more" action**: Key '3' triggers `showInfo()` with vCard explanation

3. **Add vCard configuration hint**: If user's vCard is empty, show toast:
   "Configure your vCard in vCards > My vCard"

## References
- vCard format specification: https://tools.ietf.org/html/rfc6350
- BLE proximity exchange patterns: https://www.bluetooth.com/specifications/specs/

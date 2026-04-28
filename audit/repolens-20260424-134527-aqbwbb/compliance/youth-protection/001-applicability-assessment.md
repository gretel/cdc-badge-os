---
title: "[INFO] Youth Protection (JuSchg/COPPA) Applicability Assessment"
severity: INFO
domain: compliance/youth-protection
lens: youth-protection
labels:
  - applicability
---

## Summary

After thorough analysis of the CDC Badge OS codebase, **youth protection regulations (JuSchg in Germany, COPPA in US) are NOT directly applicable** to this firmware.

**Evidence:**
- **File analyzed:** `README.md`, `components/mod_vcard/src/VcardModule.cpp`, `components/mod_vcard/src/ble_vcard.cpp`, `web-flasher/index.html`
- **Lines examined:** Full codebase scan for registration, social features, user-generated content

## Impact

This is an **informational finding**, not a compliance gap. Understanding the applicability scope helps focus audit efforts on relevant areas.

## Evidence

CDC Badge OS is a **hardware security key firmware** with the following characteristics:

1. **No User Registration/Accounts**
   - No signup flow, no age verification needed
   - Device uses PIN protection (4-8 digits) for local access only
   - See: `components/cdc_core/src/PinManager.cpp` - local PIN management

2. **No Social/Interactive Features**
   - BLE vCard exchange is direct device-to-device contact transfer
   - No chat, messaging, or user-generated content
   - See: `components/mod_vcard/src/ble_vcard.cpp` - peer-to-peer vCard exchange

3. **No Online Service**
   - All data stored locally on device (TROPIC01 secure element)
   - No backend service, no cloud synchronization
   - Web flasher (`web-flasher/index.html`) only for firmware installation

4. **No Age-Restricted Content**
   - Pure authentication/storage device (FIDO2, SSH, TOTP, passwords)
   - Similar to YubiKey or Google Titan Key

## Recommended Fix

**No action required.** This is a hardware device firmware, not a service accessible to minors with social features.

However, consider adding a **README note** clarifying the target audience if the badge is marketed:
- If targeting developers/enthusiasts: Add "Recommended for ages 13+" or similar
- If selling commercially: Ensure product description doesn't target children

## References

- **COPPA (Children's Online Privacy Protection Act):** Applies to services "directed to children under 13" with interactive features
- **JuSchg (German Youth Protection Act):** Applies to services accessible to minors with social/content features
- **GDPR Art. 8:** Parental consent for children under 16 (if online service with accounts)

Since CDC Badge OS is a **hardware device** with no online service component, these regulations do not directly apply.

---

**Assessment Date:** 2026-04-27  
**Auditor:** Youth Protection Compliance  
**Verdict:** NOT APPLICABLE (Hardware firmware, no social features, no user accounts)

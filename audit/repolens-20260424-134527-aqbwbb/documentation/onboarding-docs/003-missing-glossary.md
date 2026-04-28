---
title: "[LOW] Missing glossary of domain terms"
severity: LOW
domain: developer-onboarding
lens: onboarding-docs
labels:
  - "audit:documentation/onboarding-docs"
---

## Summary
The codebase uses many domain-specific terms and acronyms that are not defined in a central glossary:
- CDC Badge
- TROPIC01
- R-Memory slots
- ECC slots
- AAGUID
- SAO (Shitty Add-On)
- CCID
- GATT
- NVS
- PSRAM
- VCard
- FIDO2/WebAuthn/U2F (differences)
- PW1/PW3 (GPG PINs)

While some terms are explained inline, new developers must search through multiple documents to understand what they mean.

**Evidence:**
- `README.md` uses terms like "TROPIC01 secure element", "R-Memory slots", "ECC slots", "AAGUID" without defining them
- `docs/MODULE_DEVELOPMENT.md` uses "ECC key slots", "R-Memory slots", "Module IDs" without explanation
- No dedicated glossary or terminology document

## Impact
New developers:
- Must read multiple documents to understand basic terminology
- May misunderstand concepts due to lack of clear definitions
- Waste time searching for what terms mean
- May ask questions that are answered in a glossary

## Evidence
1. Terms used without definition in `README.md`:
   - "TROPIC01 secure element" (what is it?)
   - "ECC key slots (0-31)" (what does ECC mean?)
   - "R-Memory slots (0-511, 444 bytes each)" (what is R-Memory?)
   - "AAGUID" (what is this?)
   - "SAO Detection - Shitty Add-On port" (what is SAO?)

2. Terms used without definition in `docs/MODULE_DEVELOPMENT.md`:
   - "ECC key slots"
   - "R-Memory slots"
   - "Module IDs"
   - "Attestation Key"
   - "TROPIC01 slot validation"

3. `docs/README.md` has an "Overview" section but no glossary.

## Recommended Fix
Create `docs/GLOSSARY.md` with:

1. **Hardware Terms**:
   - CDC Badge: Hardware security key with ESP32-S3, TROPIC01, E-Paper display
   - TROPIC01: Secure element chip for cryptographic key storage
   - ECC slots: 32 slots for Elliptic Curve keys (P-256, Ed25519)
   - R-Memory: 512 RAM-backed memory slots (454 bytes each) for data storage
   - PSRAM: External PSRAM for large buffers on ESP32-S3

2. **Protocol Terms**:
   - FIDO2/WebAuthn: Modern passwordless authentication standard
   - U2F: Legacy two-factor authentication (predecessor to FIDO2)
   - CCID: Smart card interface protocol (for GPG)
   - GATT: Generic Attribute Profile (Bluetooth LE data transfer)

3. **Software Terms**:
   - NVS: Non-Volatile Storage (ESP32 key-value store)
   - PSRAM: Pseudo-Static RAM (external memory)
   - AAGUID: Attestation Key Identifier (unique badge ID)

4. **Module-Specific Terms**:
   - PW1: FIDO2/GPG user PIN
   - PW3: GPG admin PIN
   - SAO: Shitty Add-On (ESP32 expansion board format)

5. **Acronyms**:
   - TOTP: Time-based One-Time Password
   - FIDO: Fast Identity Online
   - BLE: Bluetooth Low Energy
   - HID: Human Interface Device
   - CDC: Communications Device Class (USB)

**Estimated effort:** ~30-45 minutes to compile from existing docs.

## References
- [TROPIC01 Datasheet](https://www.cipru.com/tropic01)
- [FIDO Alliance](https://fidoalliance.org/)

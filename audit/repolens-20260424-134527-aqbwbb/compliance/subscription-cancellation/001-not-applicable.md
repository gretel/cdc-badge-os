---
title: "[INFO] Not Applicable: No Subscription Features Found"
severity: INFO
domain: compliance
lens: subscription-cancellation
labels:
  - "not-applicable"
---

## Summary
The **CDC Badge OS** repository is a **firmware project for a hardware security key** (ESP32-S3 based with TROPIC01 secure element). It is **not a subscription-based web service** with recurring billing or user accounts. Therefore, the German Kündigungsbutton law (BGB §312k) is **not applicable** to this codebase.

## Evidence

### Project Type
- **README.md** (line 1-3): "Modular firmware for the CDC Badge v1.0/v1.1 hardware security key featuring TROPIC01 secure element."
- **Architecture**: Embedded firmware with modules for FIDO2/WebAuthn, SSH keys, TOTP, password vault, BLE, E-Paper display.

### No Subscription Logic Found
- **Search results** for subscription-related terms returned only:
  - Generic technical terms (e.g., `EventBus::subscribe()` for event handlers in `components/cdc_core/include/cdc_core/EventBus.h:90-101`)
  - BLE GATT "subscribe" for notifications (`components/cdc_hal/src/BluetoothController.cpp:398-400`)
  - Word lists in crypto libraries (`third_party/libtropic/vendor/trezor_crypto/bip39_english.c`)
  - License text (`LICENSE.md:526`)

### No Payment/Billing Features
- **Search results** for payment/billing terms:
  - No Stripe, PayPal, or payment gateway integrations
  - No recurring billing logic
  - No user account management with plans/tiers
  - No monthly/annual subscription logic

### No Web Application
- **web-flasher/index.html**: A simple static page for flashing firmware via Web Serial API. No user accounts, no subscriptions.
- Features: Version check via GitHub API, manifest.json for esp-web-tools.

## Impact
**None** - The Kündigungsbutton law applies to subscription services accessible to German consumers. This is a one-time firmware purchase/download for a physical hardware device.

## Recommended Fix
**No action needed.** Document this finding for audit trail.

## References
- German Kündigungsbutton law: BGB §312k (requires easy cancellation for subscription services)
- Applicability criteria: Subscription or recurring payment logic, user accounts with plan/tier features

DONE

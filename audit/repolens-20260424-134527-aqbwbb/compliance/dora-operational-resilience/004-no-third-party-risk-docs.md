---
title: "[MEDIUM] No Third-Party ICT Risk Management Documentation"
severity: MEDIUM
domain: Third-Party Risk
lens: dora-third-party-risk
labels:
  - "audit:compliance/dora-operational-resilience"
---

## Summary
The repository lacks documentation for third-party ICT risk management:
- No vendor assessment for critical components (TROPIC01 secure element, ESP32-S3 MCU)
- No contractual SLAs with ICT service providers documented
- No exit strategy or data migration plan for critical vendors
- No concentration risk analysis (over-reliance on single vendor)
- No audit rights documentation for third-party ICT providers

Key searches performed:
- `grep -rn 'vendor\|supplier\|sla\|third.*party' --include='*.md'` - only found generic license text
- `find . -name '*vendor*' -o -name '*supplier*' -o -name '*sla*'` - no results

## Impact
For financial entities:
- Cannot assess supply chain risk for critical hardware components
- No vendor assessment for due diligence
- Unknown exit strategy if TROPIC01 or ESP32-S3 become unavailable
- Concentration risk if single vendor provides critical components

## Evidence
Critical dependencies without vendor assessment:
1. **TROPIC01 Secure Element** (Microchip/TROPIC)
   - 32 ECC key slots, 512 R-Memory slots
   - Critical for all key storage
   - No vendor assessment document

2. **ESP32-S3** (Espressif)
   - Main MCU with 16MB flash, PSRAM
   - Critical for all operations
   - No vendor assessment document

3. **Third-party libraries** (in `third_party/` and `managed_components/`):
   - `libtropic` - TROPIC01 driver
   - `tinyusb` - USB stack
   - No supply chain risk assessment

From `README.md` line 108-111:
```
| Component | Model |
| MCU | ESP32-S3-WROOM-1 (16MB Flash, PSRAM) |
| Secure Element | TROPIC01 |
```
These are critical components with no risk documentation.

## Recommended Fix

1. **Create `docs/VENDOR_ASSESSMENT.md`** with:
   - Assessment for TROPIC01 (Microchip)
   - Assessment for ESP32-S3 (Espressif)
   - Assessment for critical software dependencies

2. **Create `docs/SUPPLY_CHAIN_RISK.md`** with:
   - Concentration risk analysis
   - Alternative supplier identification
   - Exit strategy for each critical vendor

3. **Create `docs/THIRD_PARTY_CONTRACTS.md`** with:
   - SLA requirements for hardware vendors
   - Audit rights documentation
   - Data migration plans

## References
- DORA Regulation (EU) 2022/2554, Article 28 - ICT third-party risk management
- EBA Guidelines on ICT third-party risk management (EBA-GL-2021-07)
- ISO 27036 - Information security for supplier relationships

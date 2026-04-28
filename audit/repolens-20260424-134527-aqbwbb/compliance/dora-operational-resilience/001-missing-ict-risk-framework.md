---
title: "[HIGH] Missing ICT Risk Management Framework Documentation"
severity: HIGH
domain: ICT Risk Management
lens: dora-ict-risk-framework
labels:
  - "audit:compliance/dora-operational-resilience"
---

## Summary
The CDC Badge OS repository lacks a documented ICT risk management framework. There is no:
- Risk assessment document for the firmware and its dependencies
- Asset inventory of critical ICT systems (TROPIC01 secure element, ESP32-S3 MCU, etc.)
- Business impact analysis (BIA) for system failures
- Risk classification (critical, important, standard) for ICT assets

Key files searched: `docs/SECURITY.md` (referenced in README but does not exist), `docs/risk-assessment.md`, `docs/asset-inventory.md`

## Impact
For financial entities using this firmware as part of their ICT infrastructure:
- Cannot perform due diligence on ICT risk posture
- Missing baseline for compliance audits
- No clear understanding of critical dependencies and their risk profiles
- Difficult to justify deployment in production financial environments

## Evidence
- README.md line 7: "See [SECURITY.md](SECURITY.md) for hardening steps required before production use" - but SECURITY.md does not exist
- README.md line 250-254: Disclaimer states "This repository is a proof-of-concept / demonstrator. It may contain serious bugs, incomplete edge-case handling..."
- No risk assessment document found in `docs/` directory
- No asset inventory document found

## Recommended Fix
Create a comprehensive ICT risk management framework documentation:

1. **Create `docs/SECURITY.md`** with:
   - Security hardening checklist for production deployment
   - Known limitations and mitigations
   - Secure configuration parameters

2. **Create `docs/ICT_RISK_ASSESSMENT.md`** with:
   - Asset inventory (TROPIC01, ESP32-S3, dependencies like libtropic, TinyUSB)
   - Risk classification (critical/important/standard) for each component
   - Third-party risk assessment for key dependencies

3. **Create `docs/BUSINESS_IMPACT_ANALYSIS.md`** with:
   - Failure scenarios and their impact
   - Recovery time objectives (RTO) for different failure modes
   - Recovery point objectives (RPO) for data loss scenarios

## References
- DORA Regulation (EU) 2022/2554, Article 6 - ICT risk management
- EBA Guidelines on ICT risk management (EBA-GL-2021-07)
- ISO 27005:2018 Information technology - Security techniques - Information security risk management

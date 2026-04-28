---
title: "[HIGH] No risk assessment or threat modeling documentation"
severity: HIGH
domain: risk-management
lens: nis2
labels:
  - "audit:compliance/nis2"
---

## Summary
The repository lacks systematic cybersecurity risk assessment, threat modeling, or a risk register. NIS2 requires a "risk-analysis and security-measures" approach (Art. 20). The security controls are applied ad hoc without evidence of a risk-based approach to determine what controls are necessary and proportionally applied.

## Impact
**NIS2 Art. 20 Compliance Gap**: Without risk assessment:
- No systematic identification of single points of failure
- Attack surface not evaluated
- Security measures may be insufficient for critical functions
- No risk treatment plan to track mitigation of identified risks
- Hard to justify security decisions to auditors

## Evidence
1. **No threat model documentation**:
   - No `THREAT_MODEL.md` or similar in `/docs/`
   - Plans folder only contains implementation plans, not threat analysis
   - File listing: `docs/plans/` contains only feature implementation plans

2. **No risk register**:
   - No document tracking known risks and their mitigation status
   - No risk scoring (likelihood × impact)
   - No risk treatment plan

3. **Security controls appear ad hoc**:
   - PIN lockout (3 attempts, 60s) defined but no analysis of why these values
   - DEBUG_MODE disables lockouts - no risk assessment for production hardening
   - File: `components/cdc_core/include/cdc_core/feature_flags.h:28-30`
   ```cpp
   // Debug Mode (disables lockouts, useful for development)
   #ifndef DEBUG_MODE
   #define DEBUG_MODE 1
   #endif
   ```

4. **No attack surface documentation**:
   - No list of attack vectors (physical, USB, BLE, serial, etc.)
   - No analysis of each attack surface's threat model

## Recommended Fix
1. **Create Threat Model Document** (`docs/THREAT_MODEL.md`):
   - Use STRIDE or similar methodology
   - Document each component and its trust boundaries
   - Identify threats for each data flow
   - Include attack vectors: physical tampering, USB spoofing, BLE MITM, serial injection

2. **Create Risk Register** (`docs/RISK_REGISTER.md`):
   - List identified risks with severity scores
   - Include likelihood and impact ratings
   - Document mitigation strategies
   - Track risk status (Open, Mitigated, Accepted)

3. **Document Security Control Rationale**:
   - Explain why 3-attempt lockout (not 5, not 10)
   - Justify 60-second lockout duration
   - Document trade-offs for each security decision

4. **Create Risk Treatment Plan**:
   - Link risks to specific controls
   - Track remediation progress
   - Define risk acceptance criteria for residual risks

## References
- [NIS2 Directive Art. 20 - Risk analysis and security measures](https://eur-lex.europa.eu/eli/dir/2022/2555/oj)
- [NIST SP 800-30 Rev. 1 - Guide for Conducting Risk Assessments](https://csrc.nist.gov/publications/detail/sp/800-30/rev-1/final)
- [ENISA Threat Landscape for Digital Identity](https://www.enisa.europa.eu/publications/threat-landscape-for-digital-identity)
- [OWASP Threat Modeling Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Threat_Modeling_Cheat_Sheet.html)

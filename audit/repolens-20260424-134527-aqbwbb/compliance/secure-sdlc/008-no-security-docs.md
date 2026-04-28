---
title: "[LOW] No security documentation (threat model, threat analysis)"
severity: LOW
domain: secure-sdlc
lens: compliance
labels:
  - "documentation"
  - "security"
---

## Summary
The repository lacks formal security documentation such as a threat model, security design review, or Architecture Decision Records (ADRs) for security choices. While the README describes the security architecture, there's no systematic threat analysis or documented security decisions.

## Impact
- **Knowledge Gap**: New contributors lack context on security decisions.
- **Incomplete Coverage**: Threats may be overlooked without systematic analysis.
- **Audit Difficulty**: Security reviewers must reverse-engineer design decisions.

## Evidence
File check: No security documentation found:
- `docs/threat-model.md`
- `docs/security-design.md`
- `docs/adr/`
- `docs/ADR*`

README.md mentions security architecture but lacks:
- Threat analysis (STRIDE, PASTA)
- Attack surface documentation
- Security requirements traceability

## Recommended Fix
**Create a threat model document** (`docs/THREAT_MODEL.md`):
```markdown
# Threat Model - CDC Badge OS

## Assets
- Private keys (FIDO2, SSH, GPG, TOTP)
- PIN values
- Password vault entries

## Trust Boundaries
- TROPIC01 secure element (high trust)
- ESP32-S3 application (medium trust)
- USB host (low trust)
- Serial interface (medium trust, PIN-protected)

## Threats (STRIDE)

### Spoofing
- T1: Attacker spoofs USB HID device
- T2: Attacker spoofs serial commands

### Tampering
- T3: Attacker modifies NVS storage
- T4: Attacker modifies firmware

### Information Disclosure
- T5: PIN captured via serial sniffing
- T6: Keys extracted from ESP32 RAM

### Denial of Service
- T7: Battery drain attack
- T8: Keypad lockout abuse

## Mitigations
- T1: FIDO2 origin binding
- T5: USB CDC line buffering (not real-time)
- T6: Keys never leave TROPIC01
- T7: Deep sleep after 5 min idle
```

**Create security ADRs** (`docs/adr/001-key-storage.md`):
```markdown
# ADR 001: Key Storage in TROPIC01

## Context
Where should private keys be stored?

## Decision
All private keys stored in TROPIC01 ECC slots (0-31).

## Consequences
- Pros: Hardware tamper resistance, keys never exported
- Cons: Limited to 32 slots, requires TROPIC01 driver
```

## References
- Threat modeling guide: https://learn.microsoft.com/en-us/azure/security/develop/threat-modeling-tool
- STRIDE: https://learn.microsoft.com/en-us/azure/security/develop/threat-modeling-tool-threats
- ADR template: https://github.com/joelparkerhenderson/architecture-decision-record

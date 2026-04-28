---
title: "[LOW] Missing Incident Recovery Runbook for Common Failure Modes"
severity: LOW
domain: operational-docs
lens: documentation/operational-docs
labels:
  - "audit:documentation/operational-docs"
---

## Summary
There is no incident recovery runbook documenting step-by-step procedures for common failure modes. When the device encounters issues (boot loops, TROPIC01 desync, NVS corruption, etc.), users must ad-hoc diagnose and recover without documented procedures.

**Where it should be:** New file `docs/INCIDENTS.md` or integrated into `TROUBLESHOOTING.md`

**Current state:**
- `SERIAL_COMMANDS.md` lists diagnostic commands but no guided recovery flows
- `GPG.md` has minimal troubleshooting (lines 186-204)
- No decision trees or flowcharts for symptom-to-remediation
- `TR01_CLEANUP`, `TR01_RESYNC`, `TR01_CACHE_REBUILD` commands exist but no guidance on when to use each

## Impact
**Operational Impact:**
- Extended downtime during recovery as users experiment with commands
- Risk of making wrong recovery decision (e.g., wiping TROPIC01 when resync would suffice)
- No escalation path for unresolvable issues
- Loss of institutional knowledge (each recovery starts from scratch)

**Evidence:**
- `docs/SERIAL_COMMANDS.md:68-76` - TROPIC01 recovery commands listed but not explained
- `components/cdc_log/` - Error logging exists but no guidance on interpreting errors for recovery
- No documented procedure for the most common failure: TROPIC01 session desync

## Recommended Fix
Create incident recovery procedures covering:

1. **TROPIC01 Desync** (Most Common)
   - Symptoms: `TR01_STATUS` fails, slot commands error
   - Recovery flow: `TR01_SESSION` → `TR01_RESYNC` → `TR01_CACHE_REBUILD` → `TR01_CLEANUP`
   - Decision tree for which command to try first

2. **NVS Corruption**
   - Symptoms: Random crashes, settings lost, PIN not found
   - Recovery: `NVS_READ` to diagnose, `NVS_CLEAR` for full reset
   - Data loss assessment

3. **Boot Loop**
   - Symptoms: Device resets repeatedly, USB enumerates then disconnects
   - Recovery: Serial console attachment, `ERROR_LOG` review, safe mode entry

4. **PIN Recovery**
   - Symptoms: Lost PIN, stuck at lock screen
   - Recovery: Physical reset procedure (if exists), factory reset steps

5. **USB Connection Loss**
   - Symptoms: Device not detected, intermittent connection
   - Recovery: Power cycle, BOOT+RESET, port permissions check

6. **FIDO2/GPG State Corruption**
   - Symptoms: Keys missing, credentials not found
   - Recovery: Module-specific reset commands, re-initialization

Each procedure should include:
- Symptom checklist
- Diagnostic commands
- Recovery steps (numbered, exact commands)
- Verification steps
- Data loss assessment
- Escalation criteria (when to factory reset)

## References
- `docs/SERIAL_COMMANDS.md` - Available recovery commands
- `components/cdc_log/include/cdc_log.h` - Error log structure
- `main/tropic_slot_map.h` - Expected storage state

---
**Related Issues:**
- Missing troubleshooting guide (#001)
- Missing backup/restore procedures (#002)

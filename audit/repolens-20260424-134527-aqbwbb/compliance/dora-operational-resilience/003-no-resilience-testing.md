---
title: "[MEDIUM] No Resilience Testing Documentation or Automation"
severity: MEDIUM
domain: Resilience Testing
lens: dora-resilience-testing
labels:
  - "audit:compliance/dora-operational-resilience"
---

## Summary
The repository lacks:
- Documentation of penetration testing (annual requirement under DORA)
- Disaster recovery testing procedures
- Failover testing (device recovery modes)
- Backup restoration testing
- Chaos engineering or scenario-based testing

Key searches performed:
- `grep -rn 'pentest\|penetration\|failover\|disaster.*recovery\|chaos\|red.*team'` - no results
- `find . -name '*.yml' -path '.github/workflows/*'` - only build workflow exists, no testing workflows

## Impact
For financial entities:
- Cannot demonstrate annual penetration testing requirement
- No evidence of resilience testing for production deployment
- Unknown reliability of recovery procedures
- No automated testing of failure scenarios in CI/CD

## Evidence
- `.github/workflows/build.yml` - Only builds firmware, no test execution
- `test/` directory - Contains only 3 basic link tests (`test_vcard_module_link`, `test_vcard_store`, `test_ble_vcard_symbols`)
- No test coverage for:
  - PIN lockout recovery
  - TROPIC01 failure recovery
  - Power loss during key generation
  - NVS corruption and recovery

From `.github/workflows/build.yml` (lines 18-66):
```yaml
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - name: Build firmware
        run: pio run
      - name: Upload firmware artifacts
        uses: actions/upload-artifact@v4
```
No test execution, no validation steps.

## Recommended Fix

1. **Create `docs/PENETRATION_TESTING.md`** with:
   - Test scope and methodology
   - Findings from last penetration test
   - Remediation status

2. **Create `docs/DISASTER_RECOVERY.md`** with:
   - Device recovery procedures
   - Firmware rollback process
   - Data recovery from NVS backup

3. **Add resilience tests to `.github/workflows/`**:
   - Create `resilience-tests.yml` for automated failure scenario testing
   - Include tests for: PIN recovery, NVS corruption handling, power failure during writes

4. **Expand `test/` directory**:
   - Add unit tests for recovery paths
   - Add integration tests for failure scenarios

## References
- DORA Regulation (EU) 2022/2554, Article 11 - ICT-related incident management
- DORA Regulation (EU) 2022/2554, Article 18 - ICT third-party risk management
- NIST SP 800-115 - Technical Guide to Information Security Testing

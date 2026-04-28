---
title: "[MEDIUM] No Deployment Runbook or Release Procedure Documentation"
severity: MEDIUM
domain: deployment-safety
lens: deployment-safety
labels:
  - "audit:devops/deployment-safety"
---

## Summary

The repository lacks a formal deployment runbook or release procedure. While GitHub Actions workflows exist for building and deploying documentation, there is no documented procedure for:

1. Creating a release (versioning, changelog, testing checklist)
2. Flashing firmware to devices in production
3. Verifying a successful deployment
4. Rollback procedure if deployment fails

**Evidence:**
- No `DEPLOYMENT.md`, `RELEASE.md`, or `RUNBOOK.md` files in repository
- No `docs/DEPLOYMENT.md` or `docs/RELEASE.md`
- GitHub workflows focus on build artifacts but no deployment checklist

## Impact

**Operational Risk:**
- On-call team or developer needs tribal knowledge to deploy
- Inconsistent deployment process across team members
- Higher risk of human error during releases

**Incident Response:**
- No documented rollback procedure for failed deployments
- No verification steps to confirm successful deployment
- Troubleshooting relies on individual experience

**Scalability:**
- Difficult to onboard new team members
- Hard to automate or standardize deployment process

## Evidence

1. **Missing documentation files:**
   ```bash
   ls docs/DEPLOYMENT.md docs/RELEASE.md docs/RUNBOOK.md
   # Returns: No such file
   ```

2. **GitHub workflows** (`/.github/workflows/build.yml`, `deploy-pages.yml`):
   - Build workflow creates artifacts
   - Deploy workflow publishes documentation
   - No deployment verification or smoke test steps

3. **README.md** mentions flashing but no release procedure:
   ```
   ### Flash Pre-built Firmware
   Use the browser-based flasher at CDC Badge Web Flasher
   ```

## Recommended Fix

Create a deployment runbook (`docs/DEPLOYMENT.md`) with:

1. **Release Checklist:**
   - [ ] Update version in `platformio.ini` (`APP_VERSION`)
   - [ ] Update feature flags for production (`DEBUG_MODE=0`)
   - [ ] Run full test suite
   - [ ] Create git tag (`vX.Y.Z`)
   - [ ] GitHub Actions automatically builds and creates release

2. **Post-Release Verification:**
   - Download release binaries
   - Flash to test device
   - Verify basic functionality (PIN entry, FIDO2, TOTP)
   - Check serial logs for errors

3. **Rollback Procedure:**
   - Download previous release binaries from GitHub
   - Flash using web flasher or Python tool
   - Command: `python tools/flash_firmware.py --release vX.Y.Z`

4. **Emergency Rollback:**
   - Quick command to restore previous known-good version
   - Steps to recover from bricked device

**Estimated effort:** 1 hour to draft initial runbook

## References

- [Google SRE Release Management](https://sre.google/workbook/release-management/)
- [Atlassian Deployment Runbooks](https://www.atlassian.com/continuous-delivery/continuous-deployment/deployment-runbook)
- Existing GitHub workflows: `/.github/workflows/build.yml`, `deploy-pages.yml`

</content>
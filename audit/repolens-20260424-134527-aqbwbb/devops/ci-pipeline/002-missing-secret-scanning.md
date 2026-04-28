---
title: "[HIGH] Missing secret scanning in CI pipeline"
severity: HIGH
domain: devops
lens: ci-pipeline
labels:
  - "audit:devops/ci-pipeline"
---

## Summary
The CI pipeline lacks secret scanning to detect accidentally committed credentials, API keys, or tokens. Given the project is a hardware security key with FIDO2, SSH keys, TOTP, and password vault features, developers may inadvertently commit sensitive data like attestation keys, test certificates, or NVS secrets.

**Evidence:**
- File: `.github/workflows/build.yml` - Lines 1-90
- No `gitleaks`, `truffleHog`, or GitHub's built-in secret scanning in CI
- No pre-commit hook configuration for secret detection

## Impact
- Secrets can leak into the repository and remain undiscovered
- For a security-focused project, this is particularly critical
- Credentials in git history are hard to rotate and may have been exposed
- Attacker could extract secrets from firmware binaries if committed

## Evidence
```yaml
# build.yml has no secret scanning step
- name: Checkout repository
  uses: actions/checkout@v4
  with:
    submodules: recursive

- name: Setup Python
  uses: actions/setup-python@v5
  with:
    python-version: '3.11'

# No secret scanning between checkout and build
```

## Recommended Fix
Add a secret scanning step early in the CI pipeline:

```yaml
- name: Scan for secrets
  uses: gitleaks/gitleaks-action@v2
  env:
    GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

Or use GitHub's built-in secret scanning:
1. Enable "Secret scanning" in repository settings
2. Enable "Secret scanning push protection" to block commits with secrets

Additionally, create a `.gitleaks.toml` or `.gitleaks-ignore` file for any known false positives.

## References
- [Gitleaks GitHub Action](https://github.com/gitleaks/gitleaks-action)
- [GitHub Secret Scanning](https://docs.github.com/en/code-security/secret-scanning)
- [TruffleHog](https://github.com/trufflesecurity/trufflehog)

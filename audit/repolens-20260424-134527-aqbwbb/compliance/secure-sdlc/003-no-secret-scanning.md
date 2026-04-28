---
title: "[MEDIUM] No secret scanning in CI/CD pipeline"
severity: MEDIUM
domain: secure-sdlc
lens: compliance
labels:
  - "ci-cd"
  - "secrets"
  - "security-testing"
---

## Summary
The CI/CD pipeline lacks automated secret scanning to detect accidentally committed credentials, API keys, or tokens. Tools like Gitleaks, TruffleHog, or GitHub's built-in secret scanning can detect common patterns (AWS keys, GitHub tokens, private keys) before they reach the repository.

## Impact
- **Credential Exposure**: Developers may accidentally commit secrets that remain in git history even after deletion.
- **Compliance Gap**: SOC 2 and ISO 27001 require controls for secret management.
- **Delayed Detection**: Secrets may be exposed for days/weeks before manual discovery.

## Evidence
File: `.github/workflows/build.yml`
No secret scanning step exists. The pipeline only builds firmware without checking for secrets.

Current workflow steps:
1. Checkout repository
2. Setup Python
3. Install PlatformIO
4. Build firmware
5. Upload artifacts

Missing: Secret scanning step like:
```yaml
- name: Run Gitleaks
  uses: gitleaks/gitleaks-action@v2
```

## Recommended Fix
Add secret scanning to `build.yml`:

**Option A: Gitleaks (recommended)**
```yaml
- name: Run Gitleaks
  uses: gitleaks/gitleaks-action@v2
  env:
    GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

**Option B: TruffleHog**
```yaml
- name: Run TruffleHog
  uses: trufflesecurity/trufflehog@main
  with:
    base: 'main'
    head: 'HEAD'
```

**Option C: GitHub Secret Scanning (push protection)**
Enable in repository settings:
- Settings → Secret scanning → Enable push protection
- Settings → Secret scanning → Enable pattern detection

Add to `build.yml` for CI verification:
```yaml
- name: Check for secrets
  run: |
    pip install detect-secrets
    detect-secrets scan --all-files > .secrets.baseline
```

## References
- Gitleaks: https://github.com/gitleaks/gitleaks
- TruffleHog: https://github.com/trufflesecurity/trufflehog
- GitHub Secret Scanning: https://docs.github.com/en/code-security/secret-scanning/about-secret-scanning
- OWASP Cheat Sheet: Secrets Management

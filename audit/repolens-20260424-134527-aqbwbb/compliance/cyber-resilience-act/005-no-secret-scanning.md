---
title: "[MEDIUM] No secret scanning in CI/CD pipeline"
severity: MEDIUM
domain: cyber-resilience-act
lens: secret-detection
labels:
  - "secret-scanning"
  - "ci-cd"
  - "cra-2026"
---

## Summary
The CI/CD pipeline lacks automated secret scanning. No tools like TruffleHog, Gitleaks, Detect-Secrets, or GitGuardian are configured to detect accidentally committed secrets (API keys, passwords, certificates).

**Files affected:**
- `.github/workflows/build.yml` - No secret scanning step
- `.github/workflows/deploy-pages.yml` - No secret scanning step

## Impact
Under CRA (Article 11), manufacturers must protect credentials and keys:
- **Leaked secrets**: API keys, certificates, or passwords may be accidentally committed
- **Manual discovery**: Secrets may remain exposed until manually discovered
- **Compliance gap**: CRA requires protection of authentication data

**Potential exposure points found:**
- `components/cdc_os_ui/include/cdc_os_ui/WifiHandlers.h` - Contains `char password[65]` fields (user WiFi passwords)
- `platformio.ini` - Contains `monitor_port` with specific device path (may expose user environment)

## Recommended Fix
Add secret scanning to the CI/CD pipeline:

1. **Add Gitleaks to build.yml** (runs on pull requests):
```yaml
- name: Scan for secrets with Gitleaks
  uses: gitleaks/gitleaks-action@v2
  env:
    GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

2. **Add TruffleHog as alternative**:
```yaml
- name: Scan for secrets with TruffleHog
  uses: trufflesecurity/trufflehog@main
  with:
    base: ${{ github.event.repository.default_branch }}
    head: ${{ github.event.pull_request.head.sha }}
```

3. **Configure pre-commit hook** (optional, for developers):
```yaml
# .pre-commit-config.yaml
repos:
  - repo: https://github.com/gitleaks/gitleaks
    rev: v8.16.3
    hooks:
      - id: gitleaks
```

4. **Add .gitignore patterns** for sensitive files:
```
# Environment-specific configs
*.env
*_secrets.h
```

## References
- [EU CRA Annex I - Authentication data protection](https://digital-strategy.ec.europa.eu/en/library/cyber-resilience-act)
- [Gitleaks GitHub Action](https://github.com/gitleaks/gitleaks-action)
- [TruffleHog GitHub Action](https://github.com/trufflesecurity/trufflehog)

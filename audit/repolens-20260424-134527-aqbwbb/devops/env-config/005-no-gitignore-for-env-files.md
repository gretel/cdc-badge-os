---
title: "[LOW] Missing .env files pattern in .gitignore"
severity: LOW
domain: environment configuration
lens: env-config
labels:
  - "audit:devops/env-config"
---

## Summary
The `.gitignore` file does not include patterns for common environment configuration files like `.env`, `.env.local`, `.env.production`, etc. While the project is embedded firmware (not typical web app), this is still relevant for:
- Local build overrides
- Development environment settings
- CI/CD configuration files that might be added later

## Impact
- Risk of accidentally committing environment-specific configuration
- No guidance for developers on creating local config files
- Missing pattern for common DevOps workflows

## Evidence
File: `.gitignore` - Current content does not include `.env` patterns:
```
# macOS
.DS_Store
Thumbs.db

# IDE
.vscode/
.idea/
*.sublime-*

# Build artifacts
.pio/
build/
managed_components/
dependencies.lock
...
```

No patterns for:
- `.env`
- `.env.*`
- `.env.local`
- `*.env`

## Recommended Fix
Add to `.gitignore`:
```
# Environment files
.env
.env.local
.env.*.local
*.env
```

Also consider creating `.env.example` if any environment-specific build configuration is needed in the future.

## References
- [Standard .gitignore patterns](https://github.com/github/gitignore/blob/main/Global.gitignore)
- [12-Factor App - Config](https://12factor.net/config)

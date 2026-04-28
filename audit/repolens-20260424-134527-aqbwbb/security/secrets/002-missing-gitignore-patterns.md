---
title: "[MEDIUM] Missing .gitignore Entries for Sensitive File Types"
severity: MEDIUM
domain: secrets
lens: gitignore-coverage
labels:
  - "file-patterns"
  - "configuration"
---

## Summary
The `.gitignore` file at the root of the repository lacks comprehensive patterns to exclude common secret-containing file types. While it covers some build artifacts and IDE files, it misses critical patterns for:

- Environment files (`.env`, `.env.local`, `.env.production`)
- Private key files (`*.pem`, `*.key`, `*.p12`, `*.crt`)
- Certificate files
- Configuration files with secrets
- Credential JSON files (Google service accounts, AWS configs)

Current `.gitignore` (incomplete):
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
sdkconfig.old
sdkconfig.*.old
CMakeCache.txt
CMakeFiles/
cmake-build-*/
.worktrees/
docs/plans/

# Patch marker files
components/*/.patched

# Backup/temp files
*.bak
*.orig
*~

# Logs
*.log

# Core dumps
coredump.bin

# Legacy code
legacy/

# Web flasher local test files
web-flasher/*.bin
web-flasher/manifest.json

# Doxygen output
doxygen_output/

# Claude / AI config (not for public repo)
CLAUDE.md
agents.md
.claude/
```

## Impact
**Risk of accidental secret exposure:**
- Developers may commit `.env` files with database credentials, API keys, or cloud provider secrets
- Private keys generated during development can be accidentally committed
- No protection for common credential file patterns means secrets can slip through
- The existing TROPIC01 private keys may have been committed due to this gap
- CI/CD pipelines may pick up local credential files

## Evidence
The `.gitignore` file at `/.gitignore` lacks these common patterns:
- No `.env*` pattern (covers `.env`, `.env.local`, `.env.production`, etc.)
- No `*.pem` pattern for PEM-encoded keys and certificates
- No `*.key` pattern for private key files
- No `*.p12` or `*.pfx` for PKCS12 bundles
- No `*credentials*.json` for service account JSON files
- No `*.tfvars` for Terraform variable files with secrets
- No `sdkconfig` (without `.old`/`.old` suffix) for ESP-IDF config with possible secrets

## Recommended Fix
Update `.gitignore` to include comprehensive secret-related patterns:

```gitignore
# Environment files
.env
.env.local
.env.production
.env.*.local
.env.test
.env.dev

# Private keys and certificates
*.pem
*.key
*.p12
*.pfx
*.crt
*.cer
*.der

# Credential files
*credentials*.json
*secret*.json
*config*.json
google-service-account*.json
aws-credentials

# Terraform
*.tfvars
.terraform/
terraform.tfstate
terraform.tfstate.backup

# ESP-IDF config (may contain secrets)
sdkconfig

# Provisioning data
third_party/libtropic/tropic01_model/provisioning_data/**/*.pem
third_party/libtropic/tropic01_model/provisioning_data/**/sh0_key_pair/*.pem

# macOS extended attributes (often contain metadata)
._*

# General secrets patterns
*.secret
secrets.yml
secrets.yaml
```

## References
- [gitignore.io - ESP32 template](https://www.gitignore.io/api/esp32)
- [GitHub .gitignore templates](https://github.com/github/gitignore)
- [12-Factor App - Config](https://12factor.net/config)

---
title: "[LOW] No pre-commit hooks for automated checks"
severity: LOW
domain: secure-sdlc
lens: compliance
labels:
  - "process"
  - "code-quality"
---

## Summary
The repository lacks pre-commit hooks for automated checks (linting, secret detection, formatting). This means developers must manually remember to run these checks before committing, leading to inconsistent code quality.

## Impact
- **Inconsistent Code Quality**: Formatting and linting rules may be applied inconsistently.
- **Secrets in Commits**: Developers may accidentally commit secrets before CI catches them.
- **Slower CI**: Catching issues locally reduces CI iterations.

## Evidence
File check: No pre-commit configuration found:
- `.pre-commit-config.yaml`
- `.husky/`
- `pre-commit` in `requirements.txt`

## Recommended Fix
**Option A: Pre-commit framework (recommended)**
Create `.pre-commit-config.yaml`:
```yaml
repos:
  - repo: https://github.com/pre-commit/pre-commit-hooks
    rev: v4.5.0
    hooks:
      - id: trailing-whitespace
      - id: end-of-file-fixer
      - id: check-yaml
      - id: check-json

  - repo: https://github.com/gitleaks/gitleaks
    rev: v8.18.1
    hooks:
      - id: gitleaks

  - repo: https://github.com/pre-commit/mirrors-clang-format
    rev: v18.1.0
    hooks:
      - id: clang-format
        files: \.(c|h|cpp)$

  - repo: local
    hooks:
      - id: build-check
        name: Build firmware
        entry: ~/.platformio/penv/bin/pio run
        language: system
        pass_filenames: false
```

Install in developer workflow:
```bash
pip install pre-commit
pre-commit install
```

**Option B: Husky (if JavaScript/TypeScript used)**
For web-flasher:
```bash
cd web-flasher
npm install --save-dev husky
npx husky init
```

Add to `.husky/pre-commit`:
```bash
#!/bin/sh
npx eslint src/
npx prettier --check src/
```

## References
- Pre-commit framework: https://pre-commit.com/
- Pre-commit hooks: https://pre-commit.com/hooks.html
- Gitleaks pre-commit: https://github.com/gitleaks/gitleaks#pre-commit

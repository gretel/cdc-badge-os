---
title: "[LOW] No pre-commit hooks configured for project"
severity: LOW
domain: development-workflow
lens: quality-gates
labels:
  - "audit:toolgate/quality-gates"
---

## Summary
The project lacks a `.pre-commit-config.yaml` file in the root directory, meaning developers must manually run checks before committing. While third-party dependencies (TinyUSB) have pre-commit configs, the main project does not.

**Evidence:**
- No `.pre-commit-config.yaml` in project root
- Third-party configs exist but are not used for main code:
  - `managed_components/espressif__tinyusb/.pre-commit-config.yaml`
  - Contains hooks for: check-yaml, trailing-whitespace, end-of-file-fixer, codespell, unit-test

## Impact
- **Inconsistent quality**: Code quality depends on individual developer diligence
- **Late feedback**: Issues discovered during CI instead of at commit time
- **More CI cycles**: Developers may need multiple commits to fix simple issues

## Evidence
Third-party pre-commit config (from `managed_components/espressif__tinyusb/.pre-commit-config.yaml`):
```yaml
repos:
- repo: https://github.com/pre-commit/pre-commit-hooks
  rev: v4.4.0
  hooks:
  - id: check-yaml
  - id: trailing-whitespace
  - id: end-of-file-fixer
  - id: forbid-submodules
```

No equivalent configuration exists for the main project.

## Recommended Fix
Create `.pre-commit-config.yaml` in project root:
```yaml
repos:
  - repo: https://github.com/pre-commit/pre-commit-hooks
    rev: v4.4.0
    hooks:
      - id: trailing-whitespace
      - id: end-of-file-fixer
      - id: check-yaml
      - id: check-added-large-files

  - repo: https://github.com/pre-commit/mirrors-clang-format
    rev: v14.0.0
    hooks:
      - id: clang-format
        types_or: [c, c++]
        files: \.(cpp|h)$

  - repo: https://github.com/codespell-project/codespell
    rev: v2.2.4
    hooks:
      - id: codespell
        types: [text]
```

Then instruct developers to install:
```bash
pip install pre-commit
pre-commit install
```

## References
- [pre-commit framework](https://pre-commit.com/)
- [pre-commit hooks repository](https://github.com/pre-commit/pre-commit-hooks)

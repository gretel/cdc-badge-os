---
title: "[MEDIUM] No pre-commit hooks configured for code quality checks"
severity: MEDIUM
domain: linting
lens: code-quality/linting
labels:
  - "audit:code-quality/linting"
---

## Summary
The project has no pre-commit hooks configured to catch common issues before commits. This means formatting issues, TODO comments, and other quality checks are only caught during code review (if at all).

**Location:** No `.pre-commit-config.yaml` or `.git/hooks/` setup

## Impact
- **Late issue detection**: Code quality issues are found during code review instead of at commit time
- **Review burden**: Reviewers must manually check formatting and common issues
- **Inconsistent commits**: Commits may include whitespace changes, trailing whitespace, or other noise

## Evidence
- No `.pre-commit-config.yaml` file in root directory
- No `.git/hooks/` setup documented
- No mention of pre-commit hooks in `README.md` or `CLAUDE.md`
- Build workflow only compiles, no pre-commit validation

## Recommended Fix
Set up pre-commit hooks using the `pre-commit` framework:

1. Create `.pre-commit-config.yaml`:
   ```yaml
   repos:
     - repo: https://github.com/pre-commit/pre-commit-hooks
       rev: v4.4.0
       hooks:
         - id: trailing-whitespace
         - id: end-of-file-finder
         - id: check-yaml
         - id: check-json

     - repo: local
       hooks:
         - id: clang-format
           name: clang-format
           entry: clang-format -i
           language: system
           types: [c++]
           files: \.(cpp|h)$

   ```

2. Add setup instructions to `README.md`:
   ```bash
   pip install pre-commit
   pre-commit install
   ```

3. Add a CI step to validate pre-commit passes

## References
- [pre-commit framework](https://pre-commit.com/)
- [clang-format pre-commit hook](https://github.com/pre-commit/mirrors-clang-format)

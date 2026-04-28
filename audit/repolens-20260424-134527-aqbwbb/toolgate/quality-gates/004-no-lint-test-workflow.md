---
title: "[LOW] No dedicated lint/test workflow in CI"
severity: LOW
domain: ci-cd
lens: quality-gates
labels:
  - "audit:toolgate/quality-gates"
---

## Summary
The project only has a single `build.yml` workflow that performs basic compilation. There is no separate workflow for linting, formatting checks, or unit tests.

**Evidence:**
- Workflow files in `.github/workflows/`:
  - `build.yml` - Only builds firmware with `pio run`
  - `deploy-pages.yml` - Only generates and deploys documentation
- No workflow for:
  - Code linting (clang-format check)
  - Unit tests (if any exist)
  - Static analysis (clang-tidy, cppcheck)

## Impact
- **Limited validation**: Only syntax/basic compilation is checked
- **No regression detection**: Code quality issues may accumulate
- **Build workflow**: If build fails, all checks fail (no isolation)

## Evidence
Current `build.yml` workflow (lines 34-36):
```yaml
- name: Build firmware
  run: pio run
```

No other check commands are present in either workflow file.

## Recommended Fix
Create a new workflow `.github/workflows/quality-checks.yml`:
```yaml
name: Quality Checks

on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main]

jobs:
  lint:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      
      - name: Check formatting
        run: |
          find components/ main/ -name "*.cpp" -o -name "*.h" | \
            xargs clang-format --dry-run --Werror

  # Optional: Add static analysis
  static-analysis:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Run cppcheck
        run: |
          sudo apt-get install cppcheck
          cppcheck --enable=all --std=c++17 components/ main/
```

## References
- [GitHub Actions workflow syntax](https://docs.github.com/en/actions/using-workflows/workflow-syntax-for-github-actions)
- [clang-format GitHub Action](https://github.com/marketplace/actions/clang-format-check)
- [Cppcheck documentation](http://cppcheck.sourceforge.net/)

---
title: "[MEDIUM] Doxygen documentation not validated in CI"
severity: MEDIUM
domain: ci-cd
lens: quality-gates
labels:
  - "audit:toolgate/quality-gates"
  - "documentation"
---

## Summary
Doxygen documentation build only occurs in the `deploy-pages.yml` workflow (on release/push to main), not in the regular build workflow. This means documentation errors are only caught when deploying, not on every pull request.

**Evidence:**
- `build.yml` (line 35): Only runs `pio run` (firmware build)
- `deploy-pages.yml` (line 68): Runs `doxygen Doxyfile`
- Doxygen is not a gate for regular code changes

**Current workflow structure:**
```yaml
# build.yml - runs on PR, push to main/develop
- name: Build firmware
  run: pio run

# deploy-pages.yml - runs on release/publish
- name: Build Doxygen documentation
  run: doxygen Doxyfile
```

## Impact
- Documentation errors only detected during release deployment
- Pull requests can merge with broken documentation
- Developers don't get immediate feedback on documentation quality
- Slows down release process if Doxygen fails at the end

## Recommended Fix
Add a documentation check step to `build.yml`:

```yaml
- name: Install Doxygen
  run: sudo apt-get install -y doxygen

- name: Build documentation
  run: doxygen Doxyfile

- name: Validate documentation
  run: |
    # Check that output directory exists with content
    if [ ! -d "doxygen_output/html" ]; then
      echo "Documentation build failed"
      exit 1
    fi
```

Alternatively, create a separate workflow that runs on PRs for faster feedback.

## References
- [GitHub Actions documentation](https://docs.github.com/en/actions)

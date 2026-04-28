---
title: "[LOW] Doxygen documentation not validated in main build workflow"
severity: LOW
domain: devops
lens: ci-pipeline
labels:
  - "audit:devops/ci-pipeline"
---

## Summary
The repository contains a `Doxyfile` for generating API documentation, and the `deploy-pages.yml` workflow builds documentation. However, the main `build.yml` workflow does not validate that Doxygen configuration is correct or that documentation can be built without errors. This means broken documentation can be merged without CI detection.

**Evidence:**
- File: `Doxyfile` - 2348 bytes, exists in root
- File: `.github/workflows/deploy-pages.yml` - Lines 61-64 (only place docs are built)
- File: `.github/workflows/build.yml` - No Doxygen validation step

## Impact
- Documentation can break without CI detection
- Deploy-pages workflow may fail on release day due to Doxygen issues
- No early warning when code changes break documentation (e.g., removed functions, changed signatures)
- Release process blocked by documentation issues instead of build issues
- For a hardware project with API docs, documentation quality matters

## Evidence
```yaml
# build.yml - No documentation validation:
- name: Build firmware
  run: pio run
- name: Upload firmware artifacts
  uses: actions/upload-artifact@v4
  with:
    path: artifacts/
# No Doxygen check

# deploy-pages.yml - Only place docs are built:
- name: Install Doxygen 1.16.1
  run: |
    # ... installation
- name: Build Doxygen documentation
  run: doxygen Doxyfile
```

## Recommended Fix
Add a documentation validation step to `build.yml`:

```yaml
- name: Install Doxygen
  run: |
    sudo apt-get update
    sudo apt-get install -y doxygen

- name: Validate documentation build
  run: doxygen Doxyfile
  continue-on-error: true  # Optional: warn but don't fail build
```

Alternatively, add a separate job that runs after build:

```yaml
docs-check:
  needs: build
  runs-on: ubuntu-latest
  steps:
    - uses: actions/checkout@v4
    
    - name: Install Doxygen
      run: |
        sudo apt-get update
        sudo apt-get install -y doxygen
    
    - name: Build documentation
      run: doxygen Doxyfile
```

This ensures:
- Documentation builds are validated on every PR
- Broken docs are caught before merge
- Release process is more predictable
- Developers get immediate feedback on doc-breaking changes

## References
- [Doxygen](https://www.doxygen.nl/)
- [GitHub Actions caching for Doxygen](https://docs.github.com/en/actions/guides/caching-dependencies-to-speed-up-workflows)

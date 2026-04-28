---
title: "[LOW] Overly broad permissions in build workflow"
severity: LOW
domain: devops
lens: ci-pipeline
labels:
  - "audit:devops/ci-pipeline"
---

## Summary
The `.github/workflows/build.yml` workflow grants `contents: write` permission at the workflow level (line 12), but only the `release` job (tagged releases) actually needs this permission. The main `build` job only needs read access. Following least-privilege principles, permissions should be scoped to individual jobs.

**Evidence:**
- File: `.github/workflows/build.yml` - Line 12
- Workflow-level permission: `contents: write`
- Build job only: checks out code, builds, uploads artifacts (needs read)
- Release job: creates GitHub release (needs write)

## Impact
- Broader permission scope than necessary
- If build job is modified to include new steps, unintended write access exists
- Security best practices recommend least-privilege permissions
- GitHub's security model encourages job-level scoping
- For a security-focused project (hardware key), CI permissions matter

## Evidence
```yaml
# Current - workflow level write permission:
permissions:
  contents: write

jobs:
  build:
    runs-on: ubuntu-latest
    # Only needs read: checkout, build, upload-artifact
    steps:
      - uses: actions/checkout@v4  # Needs read
      - name: Build firmware
        run: pio run               # No permission needed
      - uses: actions/upload-artifact@v4  # No permission needed

  release:
    # This job needs write for softprops/action-gh-release
    if: startsWith(github.ref, 'refs/tags/v')
```

## Recommended Fix
Move permission scoping to individual jobs:

```yaml
name: Build Firmware

on:
  push:
    branches: [main, master, release, develop, feature/*]
    tags: ['v*']
  pull_request:
    branches: [main, master, release]
  workflow_dispatch:

# Remove workflow-level permissions

jobs:
  build:
    runs-on: ubuntu-latest
    permissions:
      contents: read  # Only need to checkout and read tags

    steps:
      - name: Checkout repository
        uses: actions/checkout@v4
        with:
          submodules: recursive
      # ... rest of build steps

  release:
    needs: build
    runs-on: ubuntu-latest
    if: startsWith(github.ref, 'refs/tags/v')
    permissions:
      contents: write  # Need to create release

    steps:
      # ... release steps
```

## References
- [GitHub Actions permissions](https://docs.github.com/en/actions/using-workflows/workflow-commands-for-github-actions#setting-a-permission-token)
- [Least-privilege for CI/CD](https://docs.github.com/en/actions/security-for-github-actions/security-guides/automatic-token-authentication)
- [Default permissions](https://docs.github.com/en/actions/using-workflows/workflow-syntax-for-github-actions#permissions)

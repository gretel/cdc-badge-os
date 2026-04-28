---
title: "[LOW] Missing concurrency control in build workflow"
severity: LOW
domain: devops
lens: CI-pipeline
labels:
  - "audit:devops/ci-pipeline"
---

## Summary
The `.github/workflows/build.yml` workflow lacks concurrency control with `cancel-in-progress`. When multiple commits are pushed quickly to the same branch, all builds run to completion instead of canceling older runs, wasting CI resources and creating noise in the CI status.

**Evidence:**
- File: `.github/workflows/build.yml` - Lines 1-90
- No `concurrency` block defined
- Deploy-pages workflow (line 22-24) has concurrency control as a reference

## Impact
- Wasted CI minutes when developers push multiple commits rapidly
- Multiple build artifacts created for the same branch
- Confusing CI status on pull requests (multiple "in progress" checks)
- Slower feedback loop as queued builds wait for previous ones
- Cost inefficiency for repos with GitHub Actions usage limits

## Evidence
```yaml
# build.yml - No concurrency control:
name: Build Firmware

on:
  push:
    branches: [main, master, release, develop, feature/*]
  pull_request:
    branches: [main, master, release]

permissions:
  contents: write

jobs:
  build:
    runs-on: ubuntu-latest
    # No concurrency block

# Compare with deploy-pages.yml which has it:
concurrency:
  group: pages
  cancel-in-progress: true
```

## Recommended Fix
Add concurrency control to the build workflow:

```yaml
name: Build Firmware

on:
  push:
    branches: [main, master, release, develop, feature/*]
    tags: ['v*']
  pull_request:
    branches: [main, master, release]
  workflow_dispatch:

# Add concurrency block here
concurrency:
  group: ${{ github.workflow }}-${{ github.ref }}
  cancel-in-progress: true

permissions:
  contents: write
```

This ensures:
- Multiple pushes to the same branch cancel previous runs
- Tag pushes (releases) are not canceled (different ref)
- PRs from same branch group together
- Faster feedback as latest commit gets resources immediately

## References
- [GitHub Actions concurrency](https://docs.github.com/en/actions/using-jobs/using-concurrency)
- [Workflow concurrency examples](https://docs.github.com/en/actions/writing-workflows/choosing-when-your-workflow-runs/canceling-a-workflow)

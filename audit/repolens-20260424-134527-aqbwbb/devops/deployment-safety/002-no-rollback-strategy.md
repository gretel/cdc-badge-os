---
title: "[HIGH] No Rollback Strategy for Firmware Releases"
severity: HIGH
domain: deployment-safety
lens: deployment-safety
labels:
  - "audit:devops/deployment-safety"
---

## Summary
The CI/CD pipeline (`build.yml` and `deploy-pages.yml`) builds and releases firmware but has no documented or automated rollback mechanism. When a bad release is published, reverting requires manually creating a new release with a previous version's binaries.

**Files:**
- `.github/workflows/build.yml` (lines 1-90)
- `.github/workflows/deploy-pages.yml` (lines 1-139)
- `tools/flash_firmware.py` (lines 1-262)

## Impact
- **High blast radius**: A single bad release affects all users immediately
- **Manual recovery**: On-call must manually download old artifacts and re-publish
- **No one-click revert**: GitHub Releases don't have built-in rollback functionality
- **User confusion**: Latest release tag doesn't indicate which version is "stable"

## Evidence
The build workflow creates releases with:
```yaml
# build.yml:68-89
release:
  needs: build
  runs-on: ubuntu-latest
  if: startsWith(github.ref, 'refs/tags/v')
  steps:
    - name: Create Release
      uses: softprops/action-gh-release@v1
      with:
        files: artifacts/*
        generate_release_notes: true
```

The flash tool can download a specific release:
```python
# tools/flash_firmware.py:98-100
if tag == "latest":
    url = f"{GITHUB_API}/releases/latest"
else:
    url = f"{GITHUB_API}/releases/tags/{tag}"
```

But there's no workflow to programmatically "rollback" to a previous version.

## Recommended Fix
Create a rollback workflow that:
1. Accepts a target version tag as input (via `workflow_dispatch`)
2. Downloads artifacts from the specified release
3. Re-publishes them with a "rollback" tag or note
4. Optionally creates a GitHub Release with rollback notes

Example workflow structure:
```yaml
rollback:
  runs-on: ubuntu-latest
  steps:
    - name: Download previous release
      uses: actions/download-artifact@v4
      with:
        name: cdc-badge-firmware-${{ inputs.version }}
    - name: Re-release for rollback
      uses: softprops/action-gh-release@v1
      with:
        tag_name: "rollback-${{ inputs.version }}"
```

## References
- GitHub Actions workflow_dispatch: https://docs.github.com/en/actions/using-workflows/workflow-syntax-for-github-actions#onworkflow_dispatchinputs
- softprops/action-gh-release: https://github.com/softprops/action-gh-release

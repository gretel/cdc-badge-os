---
title: "[MEDIUM] Missing firmware artifact verification (checksums/signatures)"
severity: MEDIUM
domain: devops
lens: ci-pipeline
labels:
  - "audit:devops/ci-pipeline"
---

## Summary
The CI pipeline builds firmware artifacts but does not generate checksums or signatures for verification. Users downloading firmware from releases have no way to verify authenticity and integrity.

**Evidence:**
- File: `.github/workflows/build.yml` - Lines 50-65
- Artifacts uploaded without checksums
- Release job (lines 67-90) does not compute or verify checksums

## Impact
- Users cannot verify firmware integrity after download
- No protection against supply chain attacks
- No way to detect corrupted downloads
- For a security key project, firmware authenticity is critical

## Evidence
```yaml
- name: Upload firmware artifacts
  uses: actions/upload-artifact@v4
  with:
    name: cdc-badge-firmware-${{ steps.version.outputs.version }}
    path: artifacts/
    retention-days: 90
# No checksum generation
```

```yaml
- name: Create Release
  uses: softprops/action-gh-release@v1
  with:
    files: artifacts/*
    generate_release_notes: true
# No checksum verification or signing
```

## Recommended Fix
Add checksum generation and optional signing:

```yaml
- name: Generate checksums
  run: |
    cd artifacts/
    sha256sum *.bin > checksums.txt
    cat checksums.txt

- name: Upload checksums
  uses: actions/upload-artifact@v4
  with:
    name: cdc-badge-checksums-${{ steps.version.outputs.version }}
    path: artifacts/checksums.txt

# For signing (optional, requires secret):
- name: Sign firmware (optional)
  run: |
    gpg --batch --yes --passphrase "${{ secrets.FIRMWARE_SIGN_KEY }}" \
      --armor --output checksums.txt.sig checksums.txt
```

Also update release notes to include checksums for manual verification.

## References
- [GitHub Actions checksums](https://docs.github.com/en/actions/writing-workflows/choosing-what-your-workflow-does/workflow-commands-for-github-actions)
- [Supply chain security](https://slsa.dev/)

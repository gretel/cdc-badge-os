---
title: "[MEDIUM] Doxygen downloaded from arbitrary URLs without checksum verification"
severity: MEDIUM
domain: infra-reproducibility
lens: devops
labels:
  - "audit:devops/infra-reproducibility"
  - "ci-cd"
  - "security"
---

## Summary
The GitHub Actions workflow for documentation deployment downloads Doxygen 1.16.1 from arbitrary URLs without checksum verification. This introduces a supply chain risk and potential reproducibility issues.

**Files affected:**
- `.github/workflows/deploy-pages.yml:34-58` - Doxygen installation script

## Impact
- **Security risk**: Downloading binaries without checksum verification exposes the build pipeline to man-in-the-middle attacks or compromised download sources
- **Reproducibility**: If the download URLs change or the file at the URL is updated, builds may get different versions
- **CI reliability**: Multiple fallback URLs increase the chance of getting different versions

## Evidence
`.github/workflows/deploy-pages.yml:34-58`:
```yaml
- name: Install Doxygen 1.16.1
  run: |
    set -euo pipefail
    DOXYGEN_VERSION="1.16.1"
    DOXYGEN_ARCHIVE="/tmp/doxygen-${DOXYGEN_VERSION}.linux.bin.tar.gz"

    for URL in \
      "https://www.doxygen.nl/files/doxygen-${DOXYGEN_VERSION}.linux.bin.tar.gz" \
      "https://downloads.sourceforge.net/project/doxygen/rel-${DOXYGEN_VERSION}/doxygen-${DOXYGEN_VERSION}.linux.bin.tar.gz"
    do
      echo "Downloading ${URL}"
      if curl --fail --location --retry 3 --retry-delay 5 --output "${DOXYGEN_ARCHIVE}" "${URL}"; then
        break
      fi
    done
```

No checksum verification is performed after download.

## Recommended Fix
Add checksum verification after download:

```yaml
- name: Install Doxygen 1.16.1
  run: |
    set -euo pipefail
    DOXYGEN_VERSION="1.16.1"
    DOXYGEN_ARCHIVE="/tmp/doxygen-${DOXYGEN_VERSION}.linux.bin.tar.gz"
    EXPECTED_SHA256="abc123..."  # Add actual checksum

    curl --fail --location --retry 3 --retry-delay 5 \
      "https://www.doxygen.nl/files/doxygen-${DOXYGEN_VERSION}.linux.bin.tar.gz" \
      -o "${DOXYGEN_ARCHIVE}"

    # Verify checksum
    ACTUAL_SHA256=$(sha256sum "${DOXYGEN_ARCHIVE}" | awk '{print $1}')
    if [ "$ACTUAL_SHA256" != "$EXPECTED_SHA256" ]; then
      echo "Checksum mismatch! Expected: $EXPECTED_SHA256, Got: $ACTUAL_SHA256"
      exit 1
    fi
```

Or use the Ubuntu package manager for more reliable installation:
```yaml
- name: Install Doxygen
  run: sudo apt-get update && sudo apt-get install -y doxygen
```

## References
- [GitHub Actions security best practices](https://docs.github.com/en/actions/security-guides/security-hardening-for-github-actions)
- [Doxygen download page](https://www.doxygen.nl/download.html)

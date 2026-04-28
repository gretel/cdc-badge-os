---
title: "[MEDIUM] No Software Bill of Materials (SBOM) published"
severity: MEDIUM
domain: compliance/product-liability
lens: product-liability
labels:
  - sbom
  - supply-chain
---

## Summary
The project distributes pre-built firmware binaries but does not publish a Software Bill of Materials (SBOM) for each release. A single SBOM file exists only for the TinyUSB dependency, not for the complete firmware.

**Evidence:**
- `managed_components/espressif__tinyusb/sbom.yml` exists (only for TinyUSB)
- No SBOM for the complete firmware in releases
- `dependencies.lock` exists but is not a standard SBOM format

## Impact
Under the Product Liability Directive and related standards (e.g., Executive Order 14028 for software supply chain):
- Users cannot assess what components are in their firmware
- Vulnerability impact assessment is difficult (which versions are affected?)
- Missing transparency for supply chain security
- Fails emerging SBOM requirements for security-sensitive software

## Evidence
```bash
find /input/20260423-132359-oj8ayc/cdc-badge-os -name "sbom*" -o -name "*.spdx*" -o -name "*cyclonedx*" 2>/dev/null
# Only found: managed_components/espressif__tinyusb/sbom.yml
```

The `dependencies.lock` file (ESP-IDF format) contains:
```yaml
dependencies:
  espressif/led_strip:
    version: 2.5.5
  espressif/qrcode:
    version: 0.2.0
  espressif/tinyusb:
    version: 0.19.0~2
  idf:
    version: 5.5.0
```

But this is not published with releases in a standard format (SPDX or CycloneDX).

## Recommended Fix
Generate and publish SBOM with each release:

1. **Add SBOM generation to GitHub Actions** (build.yml):
   ```yaml
   - name: Generate SBOM
     uses: anchore/sbom-action@v0
     with:
       format: spdx-json
       output-file: sbom-${{ steps.version.outputs.version }}.spdx.json
   ```

2. **Upload SBOM to GitHub Releases**:
   ```yaml
   - name: Upload SBOM to release
     uses: softprops/action-gh-release@v1
     with:
       files: sbom-${{ steps.version.outputs.version }}.spdx.json
   ```

3. **Document in README**: Add link to SBOM in release notes.

Alternatively, manually generate using `cyclonedx-cli` or `anchore/syft`:
```bash
syft . -o spdx-json > sbom.spdx.json
```

## References
- NIST SBOM Requirements: https://www.nist.gov/itl/executive-order-14028/sbom
- SPDX Specification: https://spdx.github.io/spdx-spec/
- CycloneDX: https://cyclonedx.org/
- Anchore SBOM Action: https://github.com/anchore/sbom-action

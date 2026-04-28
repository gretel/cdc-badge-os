---
title: "[HIGH] Missing Software Bill of Materials (SBOM) generation"
severity: HIGH
domain: cyber-resilience-act
lens: sbom-generation
labels:
  - "sbom"
  - "dependency-management"
  - "cra-2026"
---

## Summary
The firmware build pipeline lacks automated SBOM (Software Bill of Materials) generation. No SBOM tool (Syft, CycloneDX, SPDX) is configured in the CI/CD workflows. A partial SBOM file exists only for the managed `espressif__tinyusb` component (`managed_components/espressif__tinyusb/sbom.yml`), but there is no comprehensive SBOM for the entire firmware release.

**Files affected:**
- `.github/workflows/build.yml` - No SBOM generation step
- `.github/workflows/deploy-pages.yml` - No SBOM artifact included
- `platformio.ini` - No SBOM generation script

## Impact
Under the CRA (Article 11), products placed on the EU market must include a complete list of dependencies. Without an SBOM:
- **Compliance failure**: Cannot demonstrate supply chain transparency to regulators
- **Vulnerability response**: Cannot quickly identify affected components when a CVE is discovered
- **Release artifacts**: Firmware binaries are released without accompanying dependency metadata

## Evidence
- `managed_components/espressif__tinyusb/sbom.yml` contains only a partial SBOM for one managed component
- `dependencies.lock` exists with direct dependencies (led_strip, qrcode, tinyusb, idf), but this is ESP-IDF specific and not in standard SBOM format (SPDX, CycloneDX)
- No workflow step generates or uploads SBOM artifacts alongside firmware releases

```yaml
# Current build.yml releases firmware without SBOM:
- name: Create Release
  uses: softprops/action-gh-release@v1
  with:
    files: artifacts/*
    generate_release_notes: true
# Missing: SBOM generation and upload
```

## Recommended Fix
Add SBOM generation to the release workflow:

1. **Add Syft or CycloneDX to build.yml**:
```yaml
- name: Generate SBOM
  uses: anchore/sbom-action@v0
  with:
    format: spdx-json
    output-file: cdc-badge-sbom.spdx.json

- name: Upload SBOM artifact
  uses: actions/upload-artifact@v4
  with:
    name: cdc-badge-sbom-${{ steps.version.outputs.version }}
    path: cdc-badge-sbom.spdx.json
```

2. **Include SBOM in release artifacts**:
```yaml
- name: Create Release
  uses: softprops/action-gh-release@v1
  with:
    files: |
      artifacts/*.bin
      cdc-badge-sbom.spdx.json
```

3. **Consider generating SBOM from `dependencies.lock`** using a script that converts ESP-IDF lock format to SPDX/CycloneDX.

## References
- [EU CRA Article 11 - Requirements for manufacturers](https://digital-strategy.ec.europa.eu/en/library/cyber-resilience-act)
- [NTIA SBOM Minimum Elements](https://www.ntia.doc.gov/report/2022/sbom-minimum-elements)
- [SPDX Specification](https://spdx.github.io/spdx-spec/v2.3/)
- [CycloneDX GitHub Action](https://github.com/CycloneDX/cyclonedx-github-action)

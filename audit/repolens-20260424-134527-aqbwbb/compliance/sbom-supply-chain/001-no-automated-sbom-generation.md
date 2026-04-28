---
title: "[MEDIUM] No automated SBOM generation in CI/CD pipeline"
severity: MEDIUM
domain: sbom-supply-chain
lens: compliance
labels:
  - "sbom:missing"
  - "ci:build"
---

## Summary
The repository lacks an automated SBOM (Software Bill of Materials) generation step in its CI/CD pipeline. While the project uses ESP-IDF with a `dependencies.lock` file containing component hashes, there is no standardized SBOM artifact (SPDX, CycloneDX format) generated during builds or included in releases.

**Location:** `.github/workflows/build.yml` - Build workflow does not include SBOM generation step.

## Impact
- **Compliance Risk:** EU CRA (Cyber Resilience Act) and US Executive Order 14028 increasingly require SBOMs for software distribution
- **Supply Chain Visibility:** Without a machine-readable SBOM, downstream consumers cannot quickly identify vulnerable dependencies
- **Release Artifacts:** Firmware binaries are released without accompanying SBOM documentation

## Evidence
- `dependencies.lock` exists with component hashes but is ESP-IDF specific format, not standard SBOM
- No `syft`, `cyclonedx-bom`, `spdx-tools`, or `trivy` in build workflow
- No SBOM artifacts (`.spdx`, `bom.json`, `bom.xml`) in release process
- Only a minimal `sbom.yml` exists in managed_components/espressif__tinyusb/ (upstream file, not project-generated)

```yaml
# Current build.yml release step (no SBOM):
- name: Create Release
  uses: softprops/action-gh-release@v1
  with:
    files: artifacts/*
    generate_release_notes: true
```

## Recommended Fix
Add SBOM generation to the build workflow:

1. **Add Syft SBOM generation** (recommended for ESP-IDF projects):
```yaml
- name: Generate SBOM
  uses: anchore/sbom-action@v0
  with:
    format: spdx-json
    output-file: sbom.spdx.json
```

2. **Include SBOM in release artifacts:**
```yaml
- name: Create Release
  uses: softprops/action-gh-release@v1
  with:
    files: |
      artifacts/*
      sbom.spdx.json
```

3. **Alternative:** Use CycloneDX with `cyclonedx-bom` for broader tool support

## References
- [EU Cyber Resilience Act](https://www.cisa.gov/news-events/briefs/2024/eu-cyber-resilience-act-key-things-know)
- [US Executive Order 14028](https://www.whitehouse.gov/briefing-room/presidential-actions/2021/05/12/executive-order-on-improving-the-nations-cybersecurity/)
- [Syft SBOM Action](https://github.com/anchore/sbom-action)
- [CycloneDX](https://cyclonedx.org/)
- [SPDX Specification](https://spdx.github.io/spdx-spec/v2.3/)

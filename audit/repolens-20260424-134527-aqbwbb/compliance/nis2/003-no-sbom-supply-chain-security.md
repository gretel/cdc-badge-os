---
title: "[MEDIUM] No Software Bill of Materials (SBOM) for supply chain security"
severity: MEDIUM
domain: supply-chain-security
lens: nis2
labels:
  - "audit:compliance/nis2"
---

## Summary
The repository lacks a Software Bill of Materials (SBOM) to track third-party dependencies. NIS2 Article 22 requires "supply chain security" assessment. There is no SBOM generated (Syft, CycloneDX, SPDX format), no automated dependency vulnerability scanning, and no process for evaluating security practices of critical service providers.

## Impact
**NIS2 Art. 22 Compliance Gap**: Without SBOM:
- Cannot quickly assess impact of new CVEs in dependencies
- No visibility into all third-party components
- Difficult to prove supply chain security to auditors
- No process for evaluating supplier security practices
- Critical dependencies (libtropic, ESP-IDF, TinyUSB) not formally tracked

## Evidence
1. **No SBOM file exists**:
   - No `sbom.json`, `bom.yaml`, `spdx.json` in repository
   - No SBOM generation in CI/CD workflow
   - File: `.github/workflows/build.yml` - No SBOM step

2. **Third-party dependencies not formally tracked**:
   - `third_party/libtropic/` - TROPIC01 SDK (critical for secure element)
   - `third_party/libtropic/vendor/trezor_crypto/` - Crypto library (note: warns it's "out-of-date")
   - `managed_components/espressif__tinyusb/` - USB stack
   - `components/Adafruit-GFX/` - Graphics library
   - `components/CalEPD/` - E-Paper driver
   - No central dependency manifest with versions and update policies

3. **No dependency vulnerability scanning**:
   - No Dependabot configuration (`dependabot.yml`)
   - No CI step for CVE scanning
   - No process for monitoring upstream security advisories

4. **Dependency warning in code**:
   - File: `third_party/libtropic/docs/other/supported_cfps/trezor_crypto.md`
   ```
   We strongly advise users that want to use Trezor Crypto in production applications to
   **not** use our out-of-date copy of Trezor Crypto inside `vendor/`, but use the
   version found in the [Trezor Firmware repository](https://github.com/trezor/trezor-firmware)
   instead and handle the dependency themselves.
   ```
   - Critical crypto library is a "known out-of-date" copy with no update tracking

5. **Python tools dependency**:
   - File: `tools/requirements.txt`
   ```
   esptool>=4.7
   requests>=2.28
   bleak>=0.21
   esp-coredump>=1.5
   ```
   - No version pinning or vulnerability scanning

## Recommended Fix
1. **Generate Initial SBOM**:
   - Use `syft` or `cyclonedx-cli` to generate SBOM
   - Save as `sbom.spdx.json` or `sbom.cyclonedx.json`
   - Include all third-party components with versions and licenses

2. **Add SBOM Generation to CI**:
   - Add step to `.github/workflows/build.yml`
   - Generate SBOM on each build
   - Attach as artifact for releases

3. **Configure Dependabot**:
   - Create `.github/dependabot.yml`
   - Enable for `github-actions`
   - Enable for `pip` (tools/requirements.txt)
   - Consider custom script for ESP-IDF/PlatformIO dependencies

4. **Create Dependency Management Policy**:
   - Document update frequency for each dependency
   - Define process for evaluating critical dependency updates
   - Track known vulnerabilities and their mitigations

5. **Document Critical Dependencies**:
   - Identify "critical" vs "non-critical" dependencies
   - Document supplier security posture for critical ones
   - Create backup plans for critical dependencies (e.g., libtropic)

## References
- [NIS2 Directive Art. 22 - Supply chain security](https://eur-lex.europa.eu/eli/dir/2022/2555/oj)
- [NIST SBOM Requirements](https://www.nist.gov/itl/executive-order-14028-improving-nationals-cybersecurity/nist-sbom-requirements)
- [CISA SBOM Basics](https://www.cisa.gov/sbom)
- [SPDX Specification](https://spdx.github.io/spdx-spec/v2.3/)
- [CycloneDX Specification](https://cyclonedx.org/)

---
title: "[LOW] Missing centralized third-party license attribution file"
severity: LOW
domain: sbom-supply-chain
lens: compliance
labels:
  - "license:attribution"
  - "compliance:docs"
---

## Summary
The project lacks a centralized THIRD-PARTY-LICENSES or ATTRIBUTION file that aggregates all dependency licenses. While individual component licenses exist in their respective directories, there is no single document for downstream consumers to review license compliance.

**Location:** Root directory missing `THIRD-PARTY-LICENSES.md` or `ATTRIBUTION.md`

## Impact
- **License Compliance Burden:** Users must manually collect licenses from multiple component directories
- **Apache 2.0 NOTICE Requirement:** Apache-licensed dependencies (CalEPD) require attribution in distributed works, which may be missed
- **GPLv3 Compatibility:** Main project is GPLv3 with permissive dependencies (MIT, Apache 2.0, BSD, Clear BSD) - documentation helps verify compatibility

## Evidence
- Main license: `LICENSE.md` (GNU General Public License v3.0)
- Dependencies with different licenses:
  - `managed_components/espressif__tinyusb/LICENSE` - MIT
  - `managed_components/espressif__qrcode/LICENSE` - BSD 3-Clause
  - `components/Adafruit-GFX/license.txt` - BSD 3-Clause
  - `components/CalEPD/LICENSE` - Apache 2.0 (requires NOTICE file in distributions)
  - `third_party/libtropic/LICENSE.md` - Clear BSD
  - `managed_components/espressif__led_strip/LICENSE` - BSD 3-Clause
- No `THIRD-PARTY-LICENSES.md`, `ATTRIBUTION.md`, or `NOTICE` file in root

Apache 2.0 NOTICE file requirement (from CalEPD LICENSE):
> "(d) If the Work includes a "NOTICE" text file as part of its distribution, then any Derivative Works must include a readable copy of the attribution notices contained within such NOTICE file"

## Recommended Fix
Create a `THIRD-PARTY-LICENSES.md` file in the root directory:

```markdown
# Third-Party Licenses

This project includes the following third-party software:

## ESP-IDF Components

### TinyUSB (MIT)
- Version: 0.19.0
- Source: espressif/tinyusb (ESP-IDF component registry)
- License: MIT
- Copyright: (c) 2018, hathach (tinyusb.org)

### QR Code Generator (BSD 3-Clause)
- Version: 0.2.0
- Source: espressif/idf-extra-components/qrcode
- License: BSD 3-Clause

### LED Strip Driver (BSD 3-Clause)
- Version: 2.5.5
- Source: espressif/idf-extra-components/led_strip
- License: BSD 3-Clause

## Third-Party Libraries

### Adafruit GFX Library (BSD 3-Clause)
- Version: 1.7.7
- Source: martinberlin/Adafruit-GFX-Library-ESP-IDF
- License: BSD 3-Clause

### CalEPD (Apache 2.0)
- Source: martincal/CalEPD
- License: Apache 2.0

### libtropic (Clear BSD)
- Version: Latest
- Source: tropicsquare/libtropic
- License: Clear BSD
```

## References
- [Apache 2.0 License Requirements](https://www.apache.org/licenses/LICENSE-2.0#apply)
- [SPDX License List](https://spdx.org/licenses/)
- [GPLv3 Compatibility](https://www.gnu.org/licenses/license-list.en.html)

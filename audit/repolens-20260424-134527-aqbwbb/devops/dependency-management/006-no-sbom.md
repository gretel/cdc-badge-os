---
title: "[LOW] No Software Bill of Materials (SBOM) generation"
severity: LOW
domain: devops
lens: dependency-management
labels:
  - "audit:devops/dependency-management"
---

## Summary
The project lacks a Software Bill of Materials (SBOM) to track all dependencies and their licenses. This is important for supply chain transparency, license compliance, and security vulnerability tracking.

**Files:** No SBOM generation script or configuration found

## Impact
- **License risk**: May unknowingly include copyleft or incompatible licenses
- **Security audit**: Harder to trace which dependencies are affected by new CVEs
- **Compliance**: Missing SBOM makes it harder to meet modern supply chain security requirements (e.g., Executive Order 14028)
- **Documentation**: No centralized view of all third-party components

## Evidence
Project structure shows multiple dependency sources:
- ESP-IDF components from component registry (led_strip, qrcode, tinyusb)
- Git submodules (CalEPD, libtropic, Adafruit-GFX)
- Python packages (esptool, requests, bleak, esp-coredump)
- Third-party libraries in `third_party/`

No SBOM files found:
- No `sbom.json` or `bom.json`
- No `cyclonedx` or `spdx` output
- No license audit script

ESP-IDF components have licenses:
- `managed_components/espressif__led_strip/idf_component.yml` - Reference to espressif repo
- `managed_components/espressif__qrcode/idf_component.yml` - Reference to espressif repo
- `components/Adafruit-GFX/license.txt` - BSD-style license exists
- `third_party/libtropic/LICENSE.md` - MIT license exists

## Recommended Fix
Create a simple SBOM generation workflow:

1. **Add a Python script to generate SBOM** (`tools/generate_sbom.py`):
```python
#!/usr/bin/env python3
"""Generate SBOM in SPDX format for CDC Badge OS."""
import json
from datetime import datetime

def generate_sbom():
    sbom = {
        "spdxVersion": "SPDX-2.3",
        "dataLicense": "CC0-1.0",
        "SPDXID": "SPDXRef-DOCUMENT",
        "name": "CDC Badge OS",
        "documentNamespace": f"https://example.org/cdc-badge-os/{datetime.now().isoformat()}",
        "creationInfo": {
            "created": datetime.now().isoformat(),
            "creators": ["Tool: generate_sbom.py"]
        },
        "packages": [
            {"name": "espressif/led_strip", "versionInfo": "2.5.5", "licenseConcluded": "Apache-2.0"},
            {"name": "espressif/qrcode", "versionInfo": "0.2.0", "licenseConcluded": "Apache-2.0"},
            {"name": "espressif/tinyusb", "versionInfo": "0.19.0~2", "licenseConcluded": "MIT"},
            {"name": "Adafruit-GFX", "versionInfo": "1.7.7", "licenseConcluded": "BSD-3-Clause"},
            {"name": "libtropic", "versionInfo": "git", "licenseConcluded": "MIT"},
        ]
    }
    with open("sbom.spdx.json", "w") as f:
        json.dump(sbom, f, indent=2)

if __name__ == "__main__":
    generate_sbom()
```

2. **Add to CI** (optional, for release):
```yaml
- name: Generate SBOM
  run: python tools/generate_sbom.py
- name: Upload SBOM
  uses: actions/upload-artifact@v4
  with:
    name: sbom
    path: sbom.spdx.json
```

3. **Consider using existing tools**:
- [CycloneDX](https://cyclonedx.org/) for XML/JSON SBOM
- [SPDX tools](https://spdx.dev/tools/) for SPDX format
- [syft](https://github.com/anchore/syft) for automated SBOM generation

## References
- [SPDX Specification](https://spdx.github.io/spdx-spec/v2.3/)
- [CycloneDX SBOM](https://cyclonedx.org/)
- [Executive Order 14028](https://www.whitehouse.gov/briefing-room/presidential-actions/2021/05/12/executive-order-on-improving-the-nations-cybersecurity/)
- [GitHub SBOM support](https://github.blog/2022-11-28-submit-your-software-bill-of-materials-with-github/)

---
**Related issues:** None
**Estimated effort:** 1 hour

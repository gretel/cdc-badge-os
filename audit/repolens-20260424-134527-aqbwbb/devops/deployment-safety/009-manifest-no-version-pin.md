---
title: "[LOW] Web Flasher Manifest Lacks Version Pinning and Update Detection"
severity: LOW
domain: deployment-safety
lens: deployment-safety
labels:
  - "audit:devops/deployment-safety"
---

## Summary
The web flasher manifest is generated at deploy time but doesn't include version metadata or a way to detect updates. Users can't see if they're on the latest version or easily update to a specific version.

**Files:**
- `.github/workflows/deploy-pages.yml` (lines 98-125)
- `web-flasher/index.html` (lines 25-30)

## Impact
- **User confusion**: Users don't know which version they have installed
- **No update detection**: Users must manually check GitHub for new versions
- **Version drift**: Different users may have different versions with no easy way to standardize
- **Troubleshooting difficulty**: Hard to diagnose issues without knowing firmware version

## Evidence
The manifest is generated without version metadata:
```yaml
# deploy-pages.yml:105-120
cat > _site/manifest.json << MANIFEST
{
  "name": "CDC Badge OS",
  "version": "${TAG}",
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "parts": [
        { "path": "bootloader.bin", "offset": 0 },
        { "path": "partitions.bin", "offset": 32768 },
        { "path": "firmware.bin", "offset": 65536 }
      ]
    }
  ]
}
MANIFEST
```

The web flasher shows version info but doesn't read it from manifest:
```html
<!-- web-flasher/index.html:290-305 -->
<script>
  async function fetchLatestVersion() {
    const badge = document.getElementById("version-badge");
    try {
      const resp = await fetch(
        "https://api.github.com/repos/krim404/cdc-badge-os/releases/latest"
      );
      // Shows latest release but doesn't compare to installed version
    }
  }
</script>
```

No mechanism to read current firmware version from device.

## Recommended Fix
1. **Add version to manifest**: Include firmware version string in manifest.json
2. **Add version display in web flasher**: Read manifest version and show it
3. **Add update check**: Compare installed version (read from device) with latest release

Example manifest enhancement:
```json
{
  "name": "CDC Badge OS",
  "version": "v0.5.0",
  "builds": [...],
  "metadata": {
    "build_date": "2026-04-27",
    "commit": "abc123"
  }
}
```

Add version display in web flasher:
```javascript
async function checkInstalledVersion() {
  // Read version from device via Web Serial
  const version = await readVersionFromDevice();
  const latest = await fetchLatestVersion();
  if (version !== latest) {
    showUpdateAvailable(version, latest);
  }
}
```

## References
- ESP Web Tools manifest: https://webbluetoothcg.github.io/web-serial/
- Web Serial API: https://developer.mozilla.org/en-US/docs/Web/API/Web_Serial_API

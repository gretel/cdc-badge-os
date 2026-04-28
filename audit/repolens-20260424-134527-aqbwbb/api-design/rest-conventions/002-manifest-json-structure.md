---
title: "[LOW] esp-web-tools manifest.json structure uses non-standard kebab-case for nested paths"
severity: LOW
domain: api-design
lens: rest-conventions
labels:
  - "audit:api-design/rest-conventions"
---

## Summary
In `.github/workflows/deploy-pages.yml` (lines 105-124), the `manifest.json` for esp-web-tools uses kebab-case file paths (`bootloader.bin`, `partitions.bin`, `firmware.bin`). While this follows general REST conventions, the manifest structure could benefit from more explicit resource naming that better reflects the ESP32 memory layout hierarchy.

## Impact
- **Discoverability**: Users unfamiliar with ESP32 memory layout may not understand offset significance
- **Extensibility**: Adding more partitions (e.g., OTA slots) would require careful naming decisions
- **Consistency**: ESP-IDF typically uses snake_case for partition names in CSV files

## Evidence
```yaml
# .github/workflows/deploy-pages.yml:105-124
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

Current naming uses simple kebab-case but lacks semantic hierarchy.

## Recommended Fix
Consider using more descriptive paths that reflect the memory layout:

```json
{
  "name": "CDC Badge OS",
  "version": "${TAG}",
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "parts": [
        { "path": "esp32/bootloader.bin", "offset": 0 },
        { "path": "esp32/partitions.bin", "offset": 32768 },
        { "path": "esp32/app/firmware.bin", "offset": 65536 }
      ]
    }
  ]
}
```

Alternatively, align with ESP-IDF partition naming conventions (snake_case in CSV):

```json
{
  "parts": [
    { "path": "esp32_s3/bootloader.bin", "offset": 0 },
    { "path": "esp32_s3/partitions.bin", "offset": 32768 },
    { "path": "esp32_s3/app.bin", "offset": 65536 }
  ]
}
```

## References
- [ESP Web Tools Manifest Format](https://espressif.github.io/esp-web-tools/manifest/)
- [ESP-IDF Partition Table Format](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/partition-tables.html)
- [RFC 3986 - Uniform Resource Identifiers](https://tools.ietf.org/html/rfc3986) (path segments)

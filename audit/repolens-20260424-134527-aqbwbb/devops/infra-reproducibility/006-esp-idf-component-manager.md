---
title: "[LOW] ESP-IDF Component Manager dependencies not explicitly versioned"
severity: LOW
domain: infra-reproducibility
lens: devops
labels:
  - "audit:devops/infra-reproducibility"
  - "dependencies"
---

## Summary
The project uses ESP-IDF Component Manager (`components/usb_badge/idf_component.yml`, `components/Adafruit-GFX/idf_component.yml`) which pulls dependencies dynamically. While `dependencies.lock` exists, it is currently excluded from git in `.gitignore`.

**Files affected:**
- `.gitignore` - Excludes `dependencies.lock`
- `components/usb_badge/idf_component.yml` - TinyUSB dependency
- `components/Adafruit-GFX/idf_component.yml` - LED strip, QR code dependencies
- `dependencies.lock` - Contains lock data but not committed

## Impact
- **Reproducibility**: Without committing `dependencies.lock`, each build may pull different component versions
- **Build consistency**: CI and local builds may use different component versions
- **Debugging**: Harder to trace which component version caused a build change

## Evidence
`.gitignore`:
```
# Build artifacts
.pio/
build/
managed_components/
dependencies.lock
```

Note: `dependencies.lock` is listed as a build artifact and excluded from git, but it's the lock file for ESP-IDF components.

`dependencies.lock` (current content shows it exists and has data):
```yaml
dependencies:
  espressif/led_strip:
    version: 2.5.5
  espressif/qrcode:
    version: 0.2.0
  espressif/tinyusb:
    version: 0.19.0~2
```

## Recommended Fix
1. **Move `dependencies.lock` out of the build artifacts exclusion** in `.gitignore`:
   ```
   # Build artifacts
   .pio/
   build/
   managed_components/
   sdkconfig.old
   # Note: dependencies.lock should be committed!
   ```

2. **Commit `dependencies.lock`** to ensure reproducible builds:
   ```bash
   git add dependencies.lock
   git commit -m "Add ESP-IDF component lock file"
   ```

3. **Verify CI uses the locked versions**:
   The ESP-IDF framework should automatically use `dependencies.lock` when present.

## References
- [ESP-IDF Component Manager](https://components.espressif.com/)
- [ESP-IDF dependencies.lock documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/tools/idf-component-manager.html)

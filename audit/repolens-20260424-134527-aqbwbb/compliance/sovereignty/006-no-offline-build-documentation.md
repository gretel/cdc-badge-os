---
title: "[LOW] No documentation for offline/air-gapped builds"
severity: LOW
domain: digital-sovereignty
lens: offline-builds
labels:
  - "audit:compliance/sovereignty"
---

## Summary

The project lacks documentation for building firmware in offline or air-gapped environments. While the ESP-IDF framework supports offline builds with cached components, there is no guidance in the README or docs for:

1. Pre-fetching all dependencies for offline builds
2. Creating a local component mirror
3. Building without internet access (useful for high-security environments)

## Impact

- **Low risk**: Most developers have internet access
- Important for high-security / sovereign deployments
- Enables builds in air-gapped environments (common for security keys)
- No documented procedure for reproducibility verification

## Evidence

**File**: `README.md`  
**Lines**: 140-160 (Build from Source section)

Only shows online build process:
```bash
git submodule update --init --recursive
~/.platformio/penv/bin/pio run
```

No mention of:
- Offline dependency fetching
- Local component caching
- Reproducible build verification

## Recommended Fix

Add an "Offline Builds" section to `README.md` or create `docs/OFFLINE_BUILDS.md`:

```markdown
## Offline Builds

For air-gapped environments:

1. **Fetch dependencies online first**:
   ```bash
   git submodule update --init --recursive
   ~/.platformio/penv/bin/pio run
   ```

2. **Copy cached components**:
   ```bash
   cp -r ~/.platformio/packages ~/.platformio/.penv/
   ```

3. **Build offline**:
   ```bash
   pio run --offline
   ```

4. **Verify reproducibility**:
   ```bash
   sha256sum .pio/build/cdc_badge_usb/firmware.bin
   ```
```

## References

- [ESP-IDF Offline Builds](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html)
- [Reproducible Builds](https://reproducible-builds.org/)

---

---
title: "[MEDIUM] DEBUG_MODE Default is Enabled (Production Risk)"
severity: MEDIUM
domain: deployment-safety
lens: deployment-safety
labels:
  - "audit:devops/deployment-safety"
---

## Summary
The `DEBUG_MODE` feature flag defaults to `1` (enabled) in `feature_flags.h`. If a developer forgets to explicitly set it to `0` for production releases, the firmware will ship with debug lockouts disabled, reducing security.

**Files:**
- `components/cdc_core/include/cdc_core/feature_flags.h` (lines 29-32)
- `platformio.ini` (lines 18-29)

## Impact
- **Security risk**: Production builds may ship with PIN lockouts disabled
- **Brute-force vulnerability**: Attackers can try unlimited PIN combinations
- **Easy to miss**: Default is "debug-friendly", requires explicit opt-out for production
- **Hard to detect**: Binary doesn't clearly indicate if DEBUG_MODE is enabled

## Evidence
```cpp
// feature_flags.h:29-32
// Debug Mode (disables lockouts, useful for development)
#ifndef DEBUG_MODE
#define DEBUG_MODE 1
#endif
```

The README documents this:
```markdown
// README.md:88-91
**Note:** `DEBUG_MODE=1` disables lockouts for development. Set to 0 for production!
```

But there's no enforcement in the CI pipeline:
```ini
// platformio.ini:18-29
build_flags =
    -std=gnu++17
    -D BUILD_TIME=__TIME__
    -D BUILD_DATE=__DATE__
    -D APP_NAME=\"CDCBos\"
    -D APP_VERSION=\"0.5.0\"
    ; No DEBUG_MODE=0 explicitly set
```

## Recommended Fix
1. **Change default to production-safe**: Set `DEBUG_MODE=0` by default
2. **Add CI check**: Verify DEBUG_MODE is set correctly in build workflow
3. **Add version string**: Include debug flag in firmware version for identification

Example changes:
```cpp
// feature_flags.h: Change default
#ifndef DEBUG_MODE
#define DEBUG_MODE 0  // Production-safe default
#endif
```

```yaml
# build.yml: Add validation step
- name: Verify build configuration
  run: |
    # Check that DEBUG_MODE is disabled for release builds
    grep -q "DEBUG_MODE=1" .pio/build/cdc_badge_usb/sdkconfig && \
      echo "WARNING: DEBUG_MODE is enabled!" || \
      echo "OK: DEBUG_MODE is disabled"
```

## References
- Security by default: https://cheatsheetseries.owasp.org/cheatsheets/Default_Settings_Cheat_Sheet.html
- ESP32 security best practices: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/security/index.html

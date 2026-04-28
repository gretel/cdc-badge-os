---
title: "[MEDIUM] Outdated Adafruit-GFX Library (v1.1.5) - Missing Security Patches"
severity: MEDIUM
domain: dependencies
lens: dependency-cves
labels:
  - "audit:security/dependency-cves"
---

## Summary
The project uses **Adafruit GFX Library version 1.1.5** (declared in `components/Adafruit-GFX/library.properties`), which is significantly outdated. The current version of Adafruit GFX Library is 1.11.5+ (as of 2024). The submodule points to `components/Adafruit-GFX` at commit `543bce4` (tag v1.7.7-6-g543bce4), which is a fork for ESP-IDF but still based on an older upstream version.

**Files affected:**
- `components/Adafruit-GFX/library.properties` (line 2: `version=1.1.5`)
- `components/Adafruit-GFX/idf_component.yml` (line 1: `version: "1.7.7"`)
- `.gitmodules` (line 10-11: submodule definition)

## Impact
1. **Missing security patches**: Older versions of display libraries can contain buffer overflow vulnerabilities in font rendering and graphics operations.
2. **Missing bug fixes**: Several minor versions have fixed memory handling issues in font rendering.
3. **Compatibility issues**: Newer ESP-IDF versions may have better compatibility with updated library versions.
4. **Maintenance burden**: Using an outdated fork makes it harder to merge upstream security fixes.

## Evidence
From `components/Adafruit-GFX/library.properties`:
```
name=Adafruit GFX Library
version=1.1.5
```

From `components/Adafruit-GFX/idf_component.yml`:
```yaml
version: "1.7.7"
```

The Adafruit-GFX-Library submodule commit:
- Commit: `543bce4dd06afa406082f4f8dfb09abe4f5a031f`
- Tag: `v1.7.7-6-g543bce4`
- Upstream Adafruit GFX v1.1.5 is from 2017; current is v1.11.5+

## Recommended Fix
1. **Update the Adafruit-GFX submodule** to the latest version:
   ```bash
   cd components/Adafruit-GFX
   git fetch origin
   git checkout main  # or latest tag
   cd ..
   git add components/Adafruit-GFX
   git commit -m "Update Adafruit-GFX to latest version"
   ```

2. **Verify compatibility** with existing code by running the build:
   ```bash
   ~/.platformio/penv/bin/pio run
   ```

3. **Test display functionality** to ensure no breaking changes affect the E-Paper display integration.

4. Consider using the official Adafruit-GFX-Library as a reference and potentially upstreaming any ESP-IDF-specific changes.

## References
- Adafruit GFX Library GitHub: https://github.com/adafruit/Adafruit-GFX-Library
- ESP-IDF Adafruit-GFX fork: https://github.com/martinberlin/Adafruit-GFX-Library-ESP-IDF
- PlatformIO Adafruit GFX component: https://components.espressif.com/components/adafruit/adafruit_gfx

---
title: "[LOW] Adafruit-GFX Submodule Uses Fork with Limited Activity"
severity: LOW
domain: dependencies
lens: dependency-cves
labels:
  - "audit:security/dependency-cves"
---

## Summary
The Adafruit-GFX component uses a fork (`https://github.com/martinberlin/Adafruit-GFX-Library-ESP-IDF`) specifically adapted for ESP-IDF. This fork may not receive timely updates from the main Adafruit-GFX repository, potentially missing security fixes and improvements.

**Files affected:**
- `.gitmodules` (line 10-11: submodule definition)
- `components/Adafruit-GFX/` - Contains forked version

## Impact
1. **Delayed security patches**: Upstream Adafruit-GFX fixes may take time to be ported to the ESP-IDF fork.
2. **Version lag**: The fork is based on Adafruit-GFX v1.1.5, while upstream is at v1.11.5+.
3. **Single maintainer risk**: The fork has limited maintainers, creating a bus factor risk.

## Evidence
From `.gitmodules`:
```ini
[submodule "components/Adafruit-GFX"]
    path = components/Adafruit-GFX
    url = https://github.com/martinberlin/Adafruit-GFX-Library-ESP-IDF
```

From `components/Adafruit-GFX/library.properties`:
```
version=1.1.5
```

From `components/Adafruit-GFX/idf_component.yml`:
```yaml
version: "1.7.7"
```

The fork's last commit was `543bce4` ("Update glcdfont.c").

## Recommended Fix
1. **Consider using ESP-IDF component registry**: Adafruit-GFX may be available via Espressif's component registry:
   ```ini
   ; In platformio.ini lib_deps
   espressif/adafruit_gfx  ; Check if available
   ```

2. **Monitor the fork for updates**: Regularly check for new releases.

3. **Consider contributing to the fork**: If specific ESP-IDF changes are needed, contribute them upstream to the fork.

4. **Alternative**: Use the official Adafruit-GFX with ESP-IDF compatibility layer if available.

## References
- Adafruit-GFX-Library-ESP-IDF: https://github.com/martinberlin/Adafruit-GFX-Library-ESP-IDF
- Official Adafruit-GFX: https://github.com/adafruit/Adafruit-GFX-Library
- ESP-IDF Component Registry: https://components.espressif.com/

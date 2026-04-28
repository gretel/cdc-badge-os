---
title: "[MEDIUM] dependencies.lock file is in .gitignore, preventing deterministic builds"
severity: MEDIUM
domain: devops
lens: dependency-management
labels:
  - "audit:devops/dependency-management"
---

## Summary
The ESP-IDF dependency lock file `dependencies.lock` is listed in `.gitignore` at line 17, causing the project to lose deterministic builds. When a lock file is ignored, each build can pull different versions of dependencies, leading to potential inconsistencies and hard-to-reproduce bugs.

**File:** `.gitignore` (line 17)
**Affected file:** `dependencies.lock`

## Impact
- **Reproducibility**: Different developers or CI builds may use different versions of ESP-IDF components (led_strip, qrcode, tinyusb)
- **Debugging**: Issues may appear sporadically depending on which dependency version was pulled
- **Release consistency**: Built firmware may differ between builds even with the same source code
- **Dependency drift**: Over time, the project may silently transition to newer (potentially breaking) versions

## Evidence
`.gitignore` content:
```
# Build artifacts
.pio/
build/
managed_components/
dependencies.lock    # <-- Here, line 17
sdkconfig.old
```

`dependencies.lock` content shows it tracks:
- `espressif/led_strip: 2.5.5`
- `espressif/qrcode: 0.2.0`
- `espressif/tinyusb: 0.19.0~2`
- `idf: 5.5.0`

## Recommended Fix
1. Remove `dependencies.lock` from `.gitignore`
2. Commit the current `dependencies.lock` file to the repository
3. Document in README or CONTRIBUTING that the lock file should be updated when changing dependencies:
   ```bash
   # After modifying dependencies in CMakeLists.txt or idf_component.yml
   idf.py fullclean
   idf.py build  # Regenerates dependencies.lock
   git add dependencies.lock
   git commit -m "Update dependencies"
   ```

## References
- [ESP-IDF Component Manager Documentation](https://docs.espressif.com/projects/esp-component-mgr/en/latest/)
- [Twelve-Factor App - Dependencies](https://12factor.net/dependencies) - Explicitly declare and lock dependencies
- [Git ignore best practices](https://git-scm.com/book/en/v2/Git-Basics-Recording-Changes-to-the-Repository)

---
**Related issues:** None
**Estimated effort:** 30 minutes

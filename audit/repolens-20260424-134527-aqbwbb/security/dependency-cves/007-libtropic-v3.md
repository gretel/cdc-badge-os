---
title: "[LOW] libtropic v3.0.0 - New Major Version with Potential Unvetted Changes"
severity: LOW
domain: dependencies
lens: dependency-cves
labels:
  - "audit:security/dependency-cves"
---

## Summary
The project uses **libtropic v3.0.0** (from `third_party/libtropic`), which is a major version release with significant refactoring including changes to crypto HAL, pairing key handling, and L3 API. Major versions often introduce breaking changes that need careful vetting.

**Files affected:**
- `third_party/libtropic/` - libtropic SDK v3.0.0
- `.gitmodules` (line 6-7: submodule definition)

## Impact
1. **New API surface**: Refactored crypto HAL and pairing key handling may have subtle security implications.
2. **Breaking changes**: v3.0.0 has renamed return values and changed API behavior.
3. **Less battle-tested**: New major versions may have undiscovered bugs.

## Evidence
From `third_party/libtropic/CMakeLists.txt`:
```cmake
project(libtropic_SDK
        VERSION 2.0.0    ; Note: CMake says 2.0.0, but tag is v3.0.0
```

From `.gitmodules`:
```ini
[submodule "third_party/libtropic"]
    path = third_party/libtropic
    url = https://github.com/tropicsquare/libtropic.git
```

Commit: `e730ebfb585483be2347e8a96d1722806ca8d2ca` (tag: v3.0.0)

Key changes in v3.0.0:
- Refactored crypto HAL
- Changed pairing key handling (removed Python cryptography dependency)
- Renamed L3 result values (e.g., `LT_L3_PAIRING_KEY_EMPTY` → `LT_L3_SLOT_EMPTY`)

## Recommended Fix
1. **Review v3.0.0 changelog**: Thoroughly review breaking changes in `CHANGELOG.md`.

2. **Verify crypto HAL implementation**: Ensure the new crypto HAL is correctly integrated.

3. **Test security-critical functionality**: Run all FIDO2, GPG, and secure element tests.

4. **Monitor for patches**: Keep an eye on libtropic releases for any security fixes.

5. **Consider pinning a specific commit** until v3.0.0 is more stable:
   ```bash
   cd third_party/libtropic
   git checkout <specific-commit>
   ```

## References
- libtropic GitHub: https://github.com/tropicsquare/libtropic
- libtropic documentation: https://tropicsquare.github.io/libtropic/latest/
- libtropic changelog: `third_party/libtropic/CHANGELOG.md`

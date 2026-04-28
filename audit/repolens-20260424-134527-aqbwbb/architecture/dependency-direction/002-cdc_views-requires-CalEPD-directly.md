---
title: "[MEDIUM] cdc_views depends directly on CalEPD concrete implementation"
severity: MEDIUM
domain: architecture/dependency-direction
lens: dependency-direction
labels:
  - "audit:architecture/dependency-direction"
---

## Summary

The `cdc_views` component has a direct dependency on `CalEPD` (the concrete E-Paper display library) in its CMakeLists.txt, creating a tight coupling between the UI views layer and the hardware-specific display implementation.

**Affected file:**
- `components/cdc_views/CMakeLists.txt:26` - `REQUIRES ... CalEPD`

**Evidence of coupling:**
The views not only depend on CalEPD at the CMake level, but also directly include and use `Gdey029T94` class (see issue 001 for details):
```cmake
# components/cdc_views/CMakeLists.txt
idf_component_register(
    ...
    REQUIRES
        cdc_ui
        cdc_core
        cdc_hal
        cdc_log
        CalEPD  # Direct dependency on concrete implementation
)
```

## Impact

1. **Cyclic dependency risk**: `cdc_hal` depends on `CalEPD` for implementation, `cdc_views` also depends on `CalEPD`
2. **Limited flexibility**: Cannot swap display implementations without modifying views
3. **Build coupling**: Views cannot be compiled without CalEPD, even for testing

## Evidence

**CMakeLists.txt dependency chain:**
```
cdc_views -> CalEPD (direct)
cdc_hal -> CalEPD (direct)
```

Both `cdc_hal` and `cdc_views` should depend on the abstraction (`IDisplay`), not the implementation (`CalEPD`).

## Recommended Fix

1. Remove `CalEPD` from `components/cdc_views/CMakeLists.txt` REQUIRES list
2. The views should only need `cdc_hal` (which provides `IDisplay` interface)
3. If `Gdey029T94` is needed for native handle access, consider:
   - Moving the forward declaration to `cdc_hal`
   - Using `void*` for native handles (already done in `IDisplay::getNativeHandle()`)

**Scope estimate:** 30 minutes

## References

- Dependency Direction: Inner layers should not depend on outer layer implementations
- Current structure: `cdc_hal` already provides the abstraction layer

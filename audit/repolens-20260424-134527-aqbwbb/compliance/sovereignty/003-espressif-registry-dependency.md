---
title: "[LOW] ESP-IDF components fetched from Espressif's Singapore registry"
severity: LOW
domain: digital-sovereignty
lens: package-registry
labels:
  - "audit:compliance/sovereignty"
---

## Summary

The project uses Espressif's component registry (`components.espressif.com`) for managing ESP-IDF dependencies. According to `dependencies.lock`, three components are fetched from this registry:

```yaml
dependencies:
  espressif/led_strip:
    source:
      registry_url: https://components.espressif.com/
  espressif/qrcode:
    source:
      registry_url: https://components.espressif.com/
  espressif/tinyusb:
    source:
      registry_url: https://components.espressif.com/
```

Espressif Systems is headquartered in Singapore (not EU or US).

## Impact

- **Low risk**: Singapore is a stable jurisdiction with good IP protection
- Not a US dependency (different legal regime than CLOUD Act)
- Components are open source and can be vendored locally
- Build can proceed offline once components are cached
- No user data flows through this registry

## Evidence

**File**: `dependencies.lock`  
**Lines**: 1-42

```yaml
dependencies:
  espressif/led_strip:
    component_hash: 28c6509a727ef74925b372ed404772aeedf11cce10b78c3f69b3c66799095e2d
    source:
      registry_url: https://components.espressif.com/
      type: service
```

**Managed components installed**:
- `managed_components/espressif__led_strip`
- `managed_components/espressif__qrcode`
- `managed_components/espressif__tinyusb`

## Recommended Fix

**Option A: Vendor components locally**

Copy the required components to a local `local_components/` directory and update `CMakeLists.txt`:

```cmake
set(EXTRA_COMPONENT_DIRS
    ${CMAKE_CURRENT_LIST_DIR}/local_components
    ${CMAKE_CURRENT_LIST_DIR}/components
)
```

**Option B: Use git submodules**

Add components as git submodules for version control and offline builds.

**Option C: Add caching proxy**

Set up a local caching proxy for the Espressif registry to survive outages.

## References

- [Espressif Component Registry](https://components.espressif.com/)
- [ESP-IDF Component Management](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/components.html)
- [Singapore jurisdiction](https://en.wikipedia.org/wiki/Singapore)

---

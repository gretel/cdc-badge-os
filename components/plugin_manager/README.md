# plugin_manager

Loads WebAssembly plugins from the `/plugins` FAT-FS partition, validates their manifests, and routes the host API surface they import from. Sits between the WAMR runtime (`components/wamr_runtime`) and the rest of the firmware.

## Files

- `include/plugin_manager/host_api.h` - canonical mirror of the SDK header from `cdc-badge-plugins`. Drift is detected by CI in both repos.
- `include/plugin_manager/plugin_lifecycle.h` - declarations of the optional lifecycle functions a plugin can export.
- `PluginManager` - discovery + lifecycle. V1 keeps one active plugin at a time.
- `PluginStorage` - VFS mount of the `plugins` partition.
- `PluginManifest` - JSON parser for `meta.json`.
- `CapabilityChecker` - load-time validation.
- `host_api_log.cpp` - `host_log` / `host_log_hex` (real, route to `cdc_log`).
- `host_api_stubs.cpp` - everything else, returning `HOST_ERR_NOT_SUPPORTED`. Replaced incrementally by `host_api_<family>.cpp` files during Phase 3.

## Status

Phase 1: structure scaffolded, FAT-FS mount works, manifest is parseable, capability check runs. Actual WAMR instantiation, host import binding, and `plugin_on_enter` flow land in Phase 2.

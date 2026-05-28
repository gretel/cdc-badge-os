# wamr_runtime

WebAssembly Micro Runtime (WAMR) packaged as an ESP-IDF component for CDC Badge OS.

## Configuration

- Classic Interpreter (no Fast-Interp, no AOT, no JIT) for the smallest code size.
- `libc-builtin` only - plugins never see a POSIX file system.
- `WASM_ENABLE_REF_TYPES = 1`, `WASM_ENABLE_BULK_MEMORY = 1` so modern Rust toolchains can target it.
- All runtime structures, bytecode, linear memory and stacks are allocated in **PSRAM** via the custom allocator in `src/wamr_psram_alloc.c`. Internal DRAM stays free for the firmware.

## Update WAMR

```bash
cd components/wamr_runtime/wasm-micro-runtime
git fetch --tags
git checkout WAMR-<X.Y.Z>
cd ../../..
git add components/wamr_runtime/wasm-micro-runtime
```

Always update both repositories (`cdc-badge-os` and `cdc-badge-plugins`) together if the host API surface changes.

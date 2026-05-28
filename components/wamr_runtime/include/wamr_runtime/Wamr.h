/**
 * \file Wamr.h
 * \brief Public init/deinit hooks for the WAMR runtime.
 *
 * Intentionally avoids including `wasm_export.h` so callers do not pull in
 * WAMR's `log_level_t` and other symbols that conflict with cdc_log. The
 * implementation file owns the WAMR include and bridges to the runtime.
 */

#pragma once

namespace cdc::wamr {

[[nodiscard]] bool init();
void               deinit() noexcept;
[[nodiscard]] bool isReady() noexcept;

}  // namespace cdc::wamr

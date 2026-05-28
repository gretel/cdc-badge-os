/**
 * \file wamr_log.cpp
 * \brief cdc_log bridge for the WAMR runtime layer.
 *
 * Lives in its own translation unit so cdc_log.h can be included without
 * pulling in WAMR's wasm_export.h (both headers define a `log_level_t`
 * typedef and would clash if combined in the same .cpp).
 */

#include "cdc_log.h"

static const char *TAG = "WAMR";

extern "C" void wamr_log_info(const char *msg)
{
    LOG_I(TAG, "%s", msg ? msg : "");
}

extern "C" void wamr_log_error(const char *msg)
{
    LOG_E(TAG, "%s", msg ? msg : "");
}

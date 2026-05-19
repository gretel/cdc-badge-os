#pragma once

#include "cdc_ui/IView.h"
#include <cstdint>

namespace cdc::mod_homeassistant {

/**
 * \brief Sequential wizard for entering the HA URL via the on-device keypad.
 *
 * Mirrors the WLAN wizard pattern: pushes T9InputView and ListView steps in
 * sequence with chained save callbacks. Stores the assembled URL in NVS
 * under `mod_homeassistant/url`.
 *
 * Token entry is intentionally not part of the wizard (T9 for ~180 chars is
 * impractical); use the `HA_TOKEN` serial command instead.
 *
 * Steps:
 *   1. Host (T9 input, e.g. "homeassistant.local" or "192.168.1.50")
 *   2. Port (T9 numeric input, default 8123)
 *   3. HTTPS yes/no (ListView)
 *   4. Skip cert verification yes/no (only if HTTPS)
 */
class HaSetupWizardView {
public:
    /**
     * \brief Starts the wizard sequence by pushing the host-entry view.
     * \param anchor View to pop back to after the wizard finishes (typically
     *               the caller's view, e.g. `HaWaitingView`). If null, the
     *               wizard falls back to a fixed number of `pop()` calls.
     */
    static void start(ui::IView* anchor = nullptr);

private:
    static void onHostEntered(const char* host);
    static void onPortEntered(const char* port);
    static void onProtocolSelected(uint16_t index, void* userData);
    static void onSslSkipSelected(uint16_t index, void* userData);
    static void finish();
};

} // namespace cdc::mod_homeassistant

#pragma once

#include "cdc_ui/IView.h"
#include "cdc_hal/IWifiController.h"
#include <cstdint>

namespace cdc::ui {

/**
 * WiFi network item for WifiListView
 */
struct WifiItem {
    char ssid[33];                          // SSID (max 32 + null)
    int8_t rssi;                            // Signal strength in dBm
    hal::WifiSecurity security;             // Encryption type
};

/**
 * WifiListView - Specialized view for WiFi network selection
 *
 * Features:
 * - Graphical signal strength bars (1-4 bars based on RSSI)
 * - Lock icon for encrypted networks
 * - Sorted by signal strength
 * - Manual network entry option
 *
 * Legacy reference: ~/GIT/cdc-badge-os-legacy/components/cdc_badge/views.cpp
 *                   view_wifi_list_render()
 */
class WifiListView : public ViewBase {
public:
    static constexpr uint8_t MAX_NETWORKS = 20;
    static constexpr uint8_t VISIBLE_ITEMS = 4;
    static constexpr uint8_t LINE_HEIGHT = 18;

    /**
     * Selection callback
     * @param index Selected item index (0 = manual entry, 1+ = network index)
     * @param item Pointer to selected WifiItem (nullptr for manual entry)
     */
    using SelectCallback = void(*)(uint16_t index, const WifiItem* item);

    /**
     * Initialize with scan results
     * @param title View title
     * @param networks Array of networks from scan
     * @param count Number of networks
     */
    void init(const char* title, const WifiItem* networks, uint8_t count);

    /**
     * Set selection callback
     */
    void setOnSelect(SelectCallback callback) { onSelect_ = callback; }

    /**
     * Get current selection index
     */
    uint16_t getSelection() const { return selection_; }

    /**
     * Get selected network (nullptr if manual entry selected)
     */
    const WifiItem* getSelectedNetwork() const;

    // IView implementation
    void render(bool partial) override;
    InputResult onKey(char key) override;
    const char* getName() const override { return "WifiListView"; }
    const char* getFooterHint() const override;

private:
    const char* title_ = nullptr;
    WifiItem networks_[MAX_NETWORKS];
    uint8_t networkCount_ = 0;
    uint16_t selection_ = 0;
    uint16_t scrollPos_ = 0;
    SelectCallback onSelect_ = nullptr;

    void navigate(bool down);
    void ensureVisible();
    void sortByRssi();

    // Drawing helpers
    void drawSignalBars(void* gfx, int x, int y, int8_t rssi, bool inverted);
    void drawLockIcon(void* gfx, int x, int y, bool inverted);
};

} // namespace cdc::ui

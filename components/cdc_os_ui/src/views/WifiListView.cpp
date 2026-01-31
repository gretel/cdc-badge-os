/**
 * WifiListView Implementation
 *
 * WiFi network list with graphical signal bars and lock icons.
 * Based on legacy: ~/GIT/cdc-badge-os-legacy/components/cdc_badge/views.cpp
 */

#include "cdc_os_ui/views/WifiListView.h"
#include "cdc_ui/I18n.h"
#include "cdc_hal/IDisplay.h"
#include <goodisplay/gdey029T94.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <cstring>
#include <algorithm>

namespace cdc::ui {

// Layout constants
static constexpr int HEADER_HEIGHT = 16;
static constexpr int FOOTER_HEIGHT = 12;
static constexpr int CONTENT_START_Y = 38;
static constexpr int SIGNAL_WIDTH = 16;
static constexpr int LOCK_WIDTH = 12;

void WifiListView::init(const char* title, const WifiItem* networks, uint8_t count) {
    title_ = title;
    networkCount_ = (count > MAX_NETWORKS) ? MAX_NETWORKS : count;

    // Copy networks
    for (uint8_t i = 0; i < networkCount_; i++) {
        networks_[i] = networks[i];
    }

    // Sort by signal strength (strongest first)
    sortByRssi();

    selection_ = 0;
    scrollPos_ = 0;
    dirty_ = true;
}

void WifiListView::sortByRssi() {
    // Simple bubble sort (small array)
    for (uint8_t i = 0; i < networkCount_ - 1; i++) {
        for (uint8_t j = i + 1; j < networkCount_; j++) {
            if (networks_[j].rssi > networks_[i].rssi) {
                WifiItem temp = networks_[i];
                networks_[i] = networks_[j];
                networks_[j] = temp;
            }
        }
    }
}

const WifiItem* WifiListView::getSelectedNetwork() const {
    // Index 0 is "manual entry"
    if (selection_ == 0) return nullptr;
    uint8_t netIdx = selection_ - 1;
    if (netIdx < networkCount_) {
        return &networks_[netIdx];
    }
    return nullptr;
}

void WifiListView::navigate(bool down) {
    // Total items = 1 (manual) + networkCount
    uint16_t totalItems = 1 + networkCount_;

    if (down) {
        if (selection_ < totalItems - 1) {
            selection_++;
            ensureVisible();
            dirty_ = true;
        }
    } else {
        if (selection_ > 0) {
            selection_--;
            ensureVisible();
            dirty_ = true;
        }
    }
}

void WifiListView::ensureVisible() {
    if (selection_ < scrollPos_) {
        scrollPos_ = selection_;
    } else if (selection_ >= scrollPos_ + VISIBLE_ITEMS) {
        scrollPos_ = selection_ - VISIBLE_ITEMS + 1;
    }
}

const char* WifiListView::getFooterHint() const {
    return tr(StringId::HINT_SELECT);
}

InputResult WifiListView::onKey(char key) {
    switch (key) {
        case '2':  // Up
            navigate(false);
            return InputResult::CONSUMED;

        case '8':  // Down
            navigate(true);
            return InputResult::CONSUMED;

        case 'Y':  // Select
        case '5':
            if (onSelect_) {
                const WifiItem* item = getSelectedNetwork();
                onSelect_(selection_, item);
            }
            return InputResult::CONSUMED;

        case 'N':  // Back
            return InputResult::REQUEST_POP;
    }

    return InputResult::IGNORED;
}

void WifiListView::drawSignalBars(void* gfxPtr, int x, int y, int8_t rssi, bool inverted) {
    auto* gfx = static_cast<Gdey029T94*>(gfxPtr);

    // Determine number of bars based on RSSI
    // Excellent: > -50 dBm (4 bars)
    // Good: -50 to -60 dBm (3 bars)
    // Fair: -60 to -70 dBm (2 bars)
    // Weak: < -70 dBm (1 bar)
    int bars;
    if (rssi > -50) bars = 4;
    else if (rssi > -60) bars = 3;
    else if (rssi > -70) bars = 2;
    else bars = 1;

    uint16_t fg = inverted ? EPD_WHITE : EPD_BLACK;

    int barWidth = 3;
    int gap = 1;
    int baseY = y + 13;  // Bottom of tallest bar

    for (int i = 0; i < 4; i++) {
        int barHeight = 4 + i * 3;  // 4, 7, 10, 13
        int bx = x + i * (barWidth + gap);
        int by = baseY - barHeight;

        if (i < bars) {
            // Filled bar
            gfx->fillRect(bx, by, barWidth, barHeight, fg);
        } else {
            // Empty bar (outline only)
            gfx->drawRect(bx, by, barWidth, barHeight, fg);
        }
    }
}

void WifiListView::drawLockIcon(void* gfxPtr, int x, int y, bool inverted) {
    auto* gfx = static_cast<Gdey029T94*>(gfxPtr);
    uint16_t fg = inverted ? EPD_WHITE : EPD_BLACK;
    uint16_t bg = inverted ? EPD_BLACK : EPD_WHITE;

    // Lock body (filled rectangle)
    gfx->fillRect(x, y + 5, 9, 7, fg);

    // Lock shackle (arc at top)
    gfx->drawRect(x + 2, y, 5, 6, fg);
    gfx->fillRect(x + 3, y + 1, 3, 4, bg);
}

void WifiListView::render(bool partial) {
    hal::IDisplay* display = hal::getDisplayInstance();
    if (!display) return;

    auto* gfx = static_cast<Gdey029T94*>(display->getNativeHandle());
    if (!gfx) return;

    int displayWidth = display->getWidth();
    int displayHeight = display->getHeight();

    if (!partial) {
        gfx->fillScreen(EPD_WHITE);
    }

    // === Header ===
    gfx->fillRect(0, 0, displayWidth, HEADER_HEIGHT, EPD_BLACK);
    gfx->setFont(nullptr);
    gfx->setTextSize(1);
    gfx->setTextColor(EPD_WHITE);
    if (title_) {
        gfx->setCursor(4, 4);
        gfx->print(title_);
    }

    // === Content ===
    gfx->setFont(&FreeMonoBold9pt7b);
    gfx->setTextSize(1);

    uint16_t totalItems = 1 + networkCount_;  // Manual entry + networks

    if (totalItems == 0) {
        gfx->setTextColor(EPD_BLACK);
        gfx->setCursor(10, CONTENT_START_Y + 12);
        gfx->print(tr(StringId::WIFI_NO_NETWORKS));
    } else {
        int y = CONTENT_START_Y;

        for (uint8_t i = 0; i < VISIBLE_ITEMS && (scrollPos_ + i) < totalItems; i++) {
            uint16_t idx = scrollPos_ + i;
            bool isSelected = (idx == selection_);

            // Selection highlight
            if (isSelected) {
                gfx->fillRect(0, y - 12, displayWidth, LINE_HEIGHT, EPD_BLACK);
                gfx->setTextColor(EPD_WHITE);
            } else {
                gfx->setTextColor(EPD_BLACK);
            }

            if (idx == 0) {
                // Manual entry option
                gfx->setCursor(22, y);
                gfx->print("+ ");
                gfx->print(tr(StringId::WIFI_ADD_MANUAL));
            } else {
                // Network entry
                uint8_t netIdx = idx - 1;
                const WifiItem& net = networks_[netIdx];

                // Signal bars
                drawSignalBars(gfx, 4, y - 11, net.rssi, isSelected);

                // SSID (truncated to fit)
                char ssidDisplay[18];  // Leave room for lock icon
                strncpy(ssidDisplay, net.ssid, 17);
                ssidDisplay[17] = '\0';

                gfx->setCursor(22, y);
                gfx->print(ssidDisplay);

                // Lock icon for encrypted networks
                if (net.security != hal::WifiSecurity::OPEN) {
                    drawLockIcon(gfx, displayWidth - 15, y - 10, isSelected);
                }
            }

            y += LINE_HEIGHT;
        }

        // Scroll indicators
        gfx->setTextColor(EPD_BLACK);
        gfx->setFont(nullptr);
        gfx->setTextSize(1);
        if (scrollPos_ > 0) {
            gfx->setCursor(displayWidth - 10, CONTENT_START_Y);
            gfx->print("^");
        }
        if (scrollPos_ + VISIBLE_ITEMS < totalItems) {
            gfx->setCursor(displayWidth - 10, CONTENT_START_Y + LINE_HEIGHT * (VISIBLE_ITEMS - 1));
            gfx->print("v");
        }
    }

    // === Footer ===
    gfx->setFont(nullptr);
    gfx->setTextSize(1);
    gfx->setTextColor(EPD_BLACK);
    const char* hint = getFooterHint();
    if (hint) {
        // Network count indicator
        char footer[64];
        snprintf(footer, sizeof(footer), "%d/%d  %s",
                 selection_ + 1, totalItems, hint);
        int16_t x1, y1;
        uint16_t w, h;
        gfx->getTextBounds(footer, 0, 0, &x1, &y1, &w, &h);
        gfx->setCursor((displayWidth - w) / 2, displayHeight - 10);
        gfx->print(footer);
    }

    dirty_ = false;
}

} // namespace cdc::ui

/**
 * LockScreenView Implementation
 *
 * Main lock screen with clock, icons, and user info.
 */

#include "cdc_os_ui/views/LockScreenView.h"
#include "cdc_ui/I18n.h"
#include "cdc_views/ContextMenuView.h"
#include "cdc_views/KeyCodes.h"
#include "cdc_hal/IDisplay.h"
#include "cdc_hal/IKeypad.h"
#include "cdc_hal/ISleepController.h"
#include "cdc_core/ModuleRegistry.h"
#include "cdc_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <goodisplay/gdey029T94.h>
#include <cdc_os_ui/fonts/FreeMonoBold9pt8b.h>
#include <cdc_os_ui/fonts/FreeMonoBold12pt8b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include "cdc_views/RenderHelpers.h"
#include <cstring>

static const char* TAG = "LockScreen";

/**
 * \brief Display layout constants.
 */
static constexpr int CLOCK_Y = 5;
static constexpr int DATE_Y = 22;
static constexpr int ICONS_Y = 5;
static constexpr int NAME_Y = 60;       // Name position (FreeFont baseline)
static constexpr int INFO_Y = 80;       // Info line 1 (small text)
static constexpr int INFO2_Y = 96;      // Info line 2 (small text)
static constexpr int BATTERY_X = 260;
static constexpr int BATTERY_Y = 5;
static constexpr int DISPLAY_WIDTH = 296;

/**
 * \brief Font size table: 5=24pt, 4=18pt, 3=12pt, 2=9pt, 1=built-in 6x8.
 */
static const GFXfont* const FONT_SIZES[] = {
    nullptr,                // Size 1: built-in 6x8 (CP437 native)
    &FreeMonoBold9pt8b,     // Size 2: 9pt (Latin-1 range, supports umlauts)
    &FreeMonoBold12pt8b,    // Size 3: 12pt (Latin-1 range, supports umlauts)
    &FreeMonoBold18pt7b,    // Size 4: 18pt (ASCII only, unused on lockscreen)
    &FreeMonoBold24pt7b,    // Size 5: 24pt (ASCII only, unused on lockscreen)
};
static constexpr int FONT_SIZE_COUNT = 5;

/**
 * \brief Battery icon dimensions.
 */
static constexpr int BAT_WIDTH = 28;
static constexpr int BAT_HEIGHT = 12;
static constexpr int BAT_TIP_WIDTH = 3;
static constexpr int BAT_TIP_HEIGHT = 6;

namespace cdc::ui {

/**
 * \brief Initializes lock-screen state fields to defaults.
 */
void LockScreenView::init() {
    memset(name_, 0, sizeof(name_));
    memset(info_, 0, sizeof(info_));
    memset(info2_, 0, sizeof(info2_));
    strcpy(clock_, "--:--");
    memset(date_, 0, sizeof(date_));
    batteryPercent_ = 0;
    statusIcons_ = StatusIcon::NONE;
    nPressStartMs_ = 0;
    dirty_ = true;
}

/**
 * \brief Sets primary display name shown on lock screen.
 * \param name Name text (nullable).
 */
void LockScreenView::setDisplayName(const char* name) {
    if (name) {
        strncpy(name_, name, MAX_TEXT_LEN - 1);
        name_[MAX_TEXT_LEN - 1] = '\0';
    } else {
        name_[0] = '\0';
    }
    dirty_ = true;
}

/**
 * \brief Sets first informational line.
 * \param info Info text (nullable).
 */
void LockScreenView::setInfo(const char* info) {
    if (info) {
        strncpy(info_, info, MAX_TEXT_LEN - 1);
        info_[MAX_TEXT_LEN - 1] = '\0';
    } else {
        info_[0] = '\0';
    }
    dirty_ = true;
}

/**
 * \brief Sets second informational line.
 * \param info2 Secondary info text (nullable).
 */
void LockScreenView::setInfo2(const char* info2) {
    if (info2) {
        strncpy(info2_, info2, MAX_TEXT_LEN - 1);
        info2_[MAX_TEXT_LEN - 1] = '\0';
    } else {
        info2_[0] = '\0';
    }
    dirty_ = true;
}

/**
 * \brief Sets clock text shown in the header.
 * \param clock Clock text (nullable).
 */
void LockScreenView::setClock(const char* clock) {
    if (clock) {
        strncpy(clock_, clock, sizeof(clock_) - 1);
        clock_[sizeof(clock_) - 1] = '\0';
    } else {
        strcpy(clock_, "--:--");
    }
    dirty_ = true;
}

/**
 * \brief Sets date text shown below clock.
 * \param date Date text (nullable).
 */
void LockScreenView::setDate(const char* date) {
    if (date) {
        strncpy(date_, date, sizeof(date_) - 1);
        date_[sizeof(date_) - 1] = '\0';
    } else {
        date_[0] = '\0';
    }
    dirty_ = true;
}

/**
 * \brief Updates battery percentage indicator.
 * \param percent Battery percentage (clamped to 0..100).
 */
void LockScreenView::setBatteryPercent(uint8_t percent) {
    if (percent > 100) percent = 100;
    if (batteryPercent_ != percent) {
        batteryPercent_ = percent;
        dirty_ = true;
    }
}

/**
 * \brief Replaces full status-icon bitmask.
 * \param icons New status icon mask.
 */
void LockScreenView::setStatusIcons(StatusIcon icons) {
    if (statusIcons_ != icons) {
        statusIcons_ = icons;
        dirty_ = true;
    }
}

/**
 * \brief Adds one status icon flag.
 * \param icon Icon bit to set.
 */
void LockScreenView::addStatusIcon(StatusIcon icon) {
    setStatusIcons(statusIcons_ | icon);
}

/**
 * \brief Removes one status icon flag.
 * \param icon Icon bit to clear.
 */
void LockScreenView::removeStatusIcon(StatusIcon icon) {
    setStatusIcons(static_cast<StatusIcon>(
        static_cast<uint16_t>(statusIcons_) & ~static_cast<uint16_t>(icon)
    ));
}

/**
 * \brief Static lock-screen instance pointer for C-style callbacks.
 */
static LockScreenView* s_lockScreenInstance = nullptr;

/**
 * \brief Handles entering lock screen and updates backlight behavior.
 * \param context Optional enter context (unused).
 */
void LockScreenView::onEnter(void* context) {
    (void)context;
    s_lockScreenInstance = this;
    nPressStartMs_ = 0;  // Reset deep sleep trigger

    // Turn off backlight when entering lock screen (unless persistent light is on)
    if ((statusIcons_ & StatusIcon::BACKLIGHT) == StatusIcon::NONE) {
        hal::IDisplay* display = hal::getDisplayInstance();
        if (display) {
            display->backlightOff();
        }
    }
    dirty_ = true;
}

/**
 * \brief Handles returning to lock screen and reapplies backlight policy.
 */
void LockScreenView::onResume() {
    // Called when returning to lock screen (e.g., via N from main menu)
    nPressStartMs_ = 0;  // Reset deep sleep trigger

    // Turn off backlight (unless persistent light is on)
    if ((statusIcons_ & StatusIcon::BACKLIGHT) == StatusIcon::NONE) {
        hal::IDisplay* display = hal::getDisplayInstance();
        if (display) {
            display->backlightOff();
        }
    }
    dirty_ = true;
}

/**
 * \brief Toggles display backlight and corresponding status icon.
 */
void LockScreenView::toggleBacklight() {
    hal::IDisplay* display = hal::getDisplayInstance();
    if (!display) return;

    if (display->isBacklightOn()) {
        display->backlightOff();
        removeStatusIcon(StatusIcon::BACKLIGHT);
    } else {
        display->backlightOn();
        addStatusIcon(StatusIcon::BACKLIGHT);
    }
    dirty_ = true;
}

/**
 * \brief Context-menu callback toggling lock-screen backlight mode.
 */
static void onLightMenuCallback() {
    if (s_lockScreenInstance) {
        s_lockScreenInstance->toggleBacklight();
    }
    hideContextMenu();
}

/**
 * \brief Storage for dynamic context-menu items contributed by modules.
 */
static constexpr uint8_t MAX_CONTEXT_ITEMS = 8;
static ContextMenuItem s_contextItems[MAX_CONTEXT_ITEMS];
static core::LockScreenContextItem s_moduleContextItems[MAX_CONTEXT_ITEMS - 1];
static uint8_t s_moduleContextCount = 0;

/**
 * \brief Wrapper callback for module context item at index 0.
 * \return void
 */
static void moduleContextCallback0() { if (s_moduleContextItems[0].callback) { s_moduleContextItems[0].callback(); } hideContextMenu(); }
/**
 * \brief Wrapper callback for module context item at index 1.
 * \return void
 */
static void moduleContextCallback1() { if (s_moduleContextItems[1].callback) { s_moduleContextItems[1].callback(); } hideContextMenu(); }
/**
 * \brief Wrapper callback for module context item at index 2.
 * \return void
 */
static void moduleContextCallback2() { if (s_moduleContextItems[2].callback) { s_moduleContextItems[2].callback(); } hideContextMenu(); }
/**
 * \brief Wrapper callback for module context item at index 3.
 * \return void
 */
static void moduleContextCallback3() { if (s_moduleContextItems[3].callback) { s_moduleContextItems[3].callback(); } hideContextMenu(); }
/**
 * \brief Wrapper callback for module context item at index 4.
 * \return void
 */
static void moduleContextCallback4() { if (s_moduleContextItems[4].callback) { s_moduleContextItems[4].callback(); } hideContextMenu(); }
/**
 * \brief Wrapper callback for module context item at index 5.
 * \return void
 */
static void moduleContextCallback5() { if (s_moduleContextItems[5].callback) { s_moduleContextItems[5].callback(); } hideContextMenu(); }
/**
 * \brief Wrapper callback for module context item at index 6.
 * \return void
 */
static void moduleContextCallback6() { if (s_moduleContextItems[6].callback) { s_moduleContextItems[6].callback(); } hideContextMenu(); }

static void (*const s_moduleCallbacks[])() = {
    moduleContextCallback0, moduleContextCallback1, moduleContextCallback2,
    moduleContextCallback3, moduleContextCallback4, moduleContextCallback5,
    moduleContextCallback6
};

/**
 * \brief Handles lock-screen key actions.
 *
 * Key `3` opens the context menu; any other key triggers unlock callback.
 *
 * \param key Pressed key code.
 * \return Input consumption result.
 */
InputResult LockScreenView::onKey(char key) {
    // KEY_BACK ('3') opens context menu for light toggle + module items
    if (key == KEY_BACK) {
        uint8_t itemCount = 0;

        // First item: Light toggle (built-in)
        s_contextItems[itemCount++] = {tr(StringId::LIGHT), onLightMenuCallback};

        // Get module items from registry
        auto& moduleReg = core::ModuleRegistry::instance();
        s_moduleContextCount = moduleReg.getLockScreenContextItems(s_moduleContextItems, MAX_CONTEXT_ITEMS - 1);

        // Add module items to context menu
        for (uint8_t i = 0; i < s_moduleContextCount && itemCount < MAX_CONTEXT_ITEMS; i++) {
            const char* label = s_moduleContextItems[i].getLabel ? s_moduleContextItems[i].getLabel() : "???";
            s_contextItems[itemCount++] = {label, s_moduleCallbacks[i]};
        }

        showContextMenu(tr(StringId::ACTIONS), s_contextItems, itemCount);
        return InputResult::CONSUMED;
    }

    // Any other key triggers unlock
    if (onUnlock_) {
        onUnlock_();
    }
    return InputResult::CONSUMED;
}

/**
 * \brief Per-tick handler for long-press deep-sleep detection.
 * \param nowMs Current uptime in milliseconds.
 */
void LockScreenView::onTick(uint32_t nowMs) {
    // Check for long-press N -> deep sleep (flight mode)
    checkDeepSleepTrigger(nowMs);
}

/**
 * \brief Detects and handles long press on `N` key to enter deep sleep.
 * \param nowMs Current uptime in milliseconds.
 */
void LockScreenView::checkDeepSleepTrigger(uint32_t nowMs) {
    auto* keypad = hal::getKeypadInstance();
    if (!keypad) return;

    bool nPressed = keypad->isKeyPressed(hal::Key::KEY_NO);

    if (nPressed) {
        if (nPressStartMs_ == 0) {
            // N key just pressed, start timing
            nPressStartMs_ = nowMs;
        } else {
            // Check if held long enough
            uint32_t elapsed = nowMs - nPressStartMs_;
            if (elapsed >= DEEP_SLEEP_HOLD_MS) {
                LOG_I(TAG, "Long-press N detected, entering deep sleep...");

                // Render deep sleep screen and push to e-paper
                renderDeepSleepScreen();

                // Turn off backlight
                auto* display = hal::getDisplayInstance();
                if (display) {
                    display->backlightOff();
                }

                // Wait 2s so user can release button without triggering wakeup
                vTaskDelay(pdMS_TO_TICKS(2000));

                // Enter deep sleep (does not return - causes reset on wake)
                auto* sleep = hal::getSleepControllerInstance();
                if (sleep) {
                    sleep->enterDeepSleep();
                }

                // Should not reach here
                nPressStartMs_ = 0;
            }
        }
    } else {
        // N released, reset timer
        nPressStartMs_ = 0;
    }
}

/**
 * \brief Returns footer hint based on current lock-screen mode.
 * \return Localized footer hint string.
 */
const char* LockScreenView::getFooterHint() const {
    if (deepSleepMode_) return tr(StringId::DEEP_SLEEP);
    return tr(StringId::PRESS_ANY_KEY);
}

/**
 * \brief Renders battery icon including charging/no-battery overlays.
 * \param gfxPtr Native graphics pointer.
 * \param x Left coordinate.
 * \param y Top coordinate.
 */
void LockScreenView::renderBattery(void* gfxPtr, int x, int y) {
    auto* gfx = static_cast<Gdey029T94*>(gfxPtr);
    // Battery outline
    gfx->drawRect(x, y, BAT_WIDTH, BAT_HEIGHT, EPD_BLACK);

    // Battery tip
    gfx->fillRect(x + BAT_WIDTH, y + (BAT_HEIGHT - BAT_TIP_HEIGHT) / 2,
                  BAT_TIP_WIDTH, BAT_TIP_HEIGHT, EPD_BLACK);

    // Fill level (skip if no battery - show empty outline)
    bool noBattery = (statusIcons_ & StatusIcon::NO_BATTERY) != StatusIcon::NONE;
    if (!noBattery) {
        int fillWidth = (batteryPercent_ * (BAT_WIDTH - 4)) / 100;
        if (fillWidth > 0) {
            gfx->fillRect(x + 2, y + 2, fillWidth, BAT_HEIGHT - 4, EPD_BLACK);
        }
    }

    // Charging indicator
    if ((statusIcons_ & StatusIcon::CHARGING) != StatusIcon::NONE) {
        // Draw lightning bolt
        gfx->drawLine(x + BAT_WIDTH / 2 + 2, y + 1, x + BAT_WIDTH / 2 - 2, y + BAT_HEIGHT / 2, EPD_BLACK);
        gfx->drawLine(x + BAT_WIDTH / 2 - 2, y + BAT_HEIGHT / 2, x + BAT_WIDTH / 2 + 2, y + BAT_HEIGHT / 2, EPD_BLACK);
        gfx->drawLine(x + BAT_WIDTH / 2 + 2, y + BAT_HEIGHT / 2, x + BAT_WIDTH / 2 - 2, y + BAT_HEIGHT - 2, EPD_BLACK);
    }

    // No battery indicator - diagonal strike-through
    if ((statusIcons_ & StatusIcon::NO_BATTERY) != StatusIcon::NONE) {
        gfx->drawLine(x - 2, y + BAT_HEIGHT + 2, x + BAT_WIDTH + BAT_TIP_WIDTH + 2, y - 2, EPD_BLACK);
        gfx->drawLine(x - 2, y + BAT_HEIGHT + 3, x + BAT_WIDTH + BAT_TIP_WIDTH + 2, y - 1, EPD_BLACK);
    }
}

/**
 * \brief Renders top-row status icons.
 * \param gfxPtr Native graphics pointer.
 * \param x Start x coordinate.
 * \param y Start y coordinate.
 */
void LockScreenView::renderStatusIcons(void* gfxPtr, int x, int y) {
    auto* gfx = static_cast<Gdey029T94*>(gfxPtr);
    int iconX = x;
    const int iconSpacing = 14;

    // Lock icon
    if ((statusIcons_ & StatusIcon::LOCK) != StatusIcon::NONE) {
        // Draw padlock
        gfx->drawRect(iconX, y + 4, 8, 6, EPD_BLACK);
        gfx->drawCircle(iconX + 4, y + 3, 3, EPD_BLACK);
        iconX -= iconSpacing;
    }

    // WiFi icon - three stacked ripple arcs with a base dot
    if ((statusIcons_ & StatusIcon::WIFI) != StatusIcon::NONE) {
        const int cx = iconX + 5;
        const int cy = y + 10;

        // Arc 3 (large): 5px horizontal cap with stepped sides
        gfx->drawLine(cx - 2, cy - 8, cx + 2, cy - 8, EPD_BLACK);
        gfx->drawPixel(cx - 3, cy - 7, EPD_BLACK);
        gfx->drawPixel(cx + 3, cy - 7, EPD_BLACK);
        gfx->drawPixel(cx - 4, cy - 6, EPD_BLACK);
        gfx->drawPixel(cx + 4, cy - 6, EPD_BLACK);

        // Arc 2 (medium): 3px horizontal cap with stepped sides
        gfx->drawLine(cx - 1, cy - 5, cx + 1, cy - 5, EPD_BLACK);
        gfx->drawPixel(cx - 2, cy - 4, EPD_BLACK);
        gfx->drawPixel(cx + 2, cy - 4, EPD_BLACK);

        // Arc 1 (small): single-pixel peak
        gfx->drawPixel(cx, cy - 2, EPD_BLACK);

        // Base dot (2x2)
        gfx->fillRect(cx, cy, 2, 2, EPD_BLACK);

        iconX -= iconSpacing;
    }

    // BLE icon
    if ((statusIcons_ & StatusIcon::BLE) != StatusIcon::NONE) {
        // Bluetooth rune
        gfx->drawLine(iconX + 4, y, iconX + 4, y + 10, EPD_BLACK);
        gfx->drawLine(iconX + 4, y, iconX + 8, y + 3, EPD_BLACK);
        gfx->drawLine(iconX + 8, y + 3, iconX + 2, y + 7, EPD_BLACK);
        gfx->drawLine(iconX + 2, y + 3, iconX + 8, y + 7, EPD_BLACK);
        gfx->drawLine(iconX + 8, y + 7, iconX + 4, y + 10, EPD_BLACK);
        iconX -= iconSpacing;
    }

    // USB icon (simplified USB trident from legacy)
    if ((statusIcons_ & StatusIcon::USB) != StatusIcon::NONE) {
        int ux = iconX, uy = y;
        // Main stem
        gfx->drawLine(ux + 5, uy + 4, ux + 5, uy + 12, EPD_BLACK);
        // Top horizontal
        gfx->drawLine(ux + 2, uy + 4, ux + 8, uy + 4, EPD_BLACK);
        // Left branch with circle
        gfx->drawLine(ux + 2, uy + 4, ux + 2, uy, EPD_BLACK);
        gfx->fillCircle(ux + 2, uy, 1, EPD_BLACK);
        // Right branch with rectangle
        gfx->drawLine(ux + 8, uy + 4, ux + 8, uy + 2, EPD_BLACK);
        gfx->fillRect(ux + 6, uy, 4, 3, EPD_BLACK);
        // Bottom arrow
        gfx->drawLine(ux + 5, uy + 12, ux + 3, uy + 10, EPD_BLACK);
        gfx->drawLine(ux + 5, uy + 12, ux + 7, uy + 10, EPD_BLACK);
        iconX -= iconSpacing;
    }

    // Backlight icon (sun)
    if ((statusIcons_ & StatusIcon::BACKLIGHT) != StatusIcon::NONE) {
        gfx->fillCircle(iconX + 4, y + 5, 2, EPD_BLACK);
        for (int i = 0; i < 8; i++) {
            int angle = i * 45;
            int dx = (angle == 0 || angle == 180) ? 4 : (angle == 90 || angle == 270) ? 0 : 3;
            int dy = (angle == 90 || angle == 270) ? 4 : (angle == 0 || angle == 180) ? 0 : 3;
            if (angle > 90 && angle < 270) dx = -dx;
            if (angle > 0 && angle < 180) dy = -dy;
            gfx->drawPixel(iconX + 4 + dx, y + 5 + dy, EPD_BLACK);
        }
        iconX -= iconSpacing;
    }

    // Sleep icons
    if ((statusIcons_ & StatusIcon::DEEP_SLEEP) != StatusIcon::NONE) {
        gfx->setCursor(iconX, y + 2);
        gfx->print("zzZ");
        iconX -= iconSpacing + 10;
    } else if ((statusIcons_ & StatusIcon::LIGHT_SLEEP) != StatusIcon::NONE) {
        gfx->setCursor(iconX, y + 2);
        gfx->print("z");
        iconX -= iconSpacing;
    }

    // Caffeinated icon (sleep inhibited) - coffee cup
    if ((statusIcons_ & StatusIcon::CAFFEINATED) != StatusIcon::NONE) {
        int cx = iconX, cy = y;
        // Cup body
        gfx->drawRect(cx, cy + 3, 8, 7, EPD_BLACK);
        // Cup handle
        gfx->drawLine(cx + 8, cy + 4, cx + 10, cy + 4, EPD_BLACK);
        gfx->drawLine(cx + 10, cy + 4, cx + 10, cy + 8, EPD_BLACK);
        gfx->drawLine(cx + 8, cy + 8, cx + 10, cy + 8, EPD_BLACK);
        // Steam (wavy lines)
        gfx->drawPixel(cx + 2, cy + 1, EPD_BLACK);
        gfx->drawPixel(cx + 3, cy, EPD_BLACK);
        gfx->drawPixel(cx + 5, cy + 1, EPD_BLACK);
        gfx->drawPixel(cx + 6, cy, EPD_BLACK);
        iconX -= iconSpacing;
    }
    (void)iconX;
}

/**
 * \brief Renders and flushes dedicated deep-sleep transition screen.
 */
void LockScreenView::renderDeepSleepScreen() {
    deepSleepMode_ = true;

    // Clear clock and status icons for minimal screen
    setClock("");
    setDate("");
    statusIcons_ = StatusIcon::NONE;

    // Render normal lockscreen (with deep sleep footer) and push to display
    render(false);

    auto* display = hal::getDisplayInstance();
    if (display) {
        display->flushSync(hal::RefreshMode::PARTIAL);
    }
}

/**
 * \brief Renders complete lock-screen layout.
 * \param partial `true` for partial redraw, `false` for full redraw.
 */
void LockScreenView::render(bool partial) {
    if (preRenderCb_) {
        preRenderCb_();
    }

    hal::IDisplay* display = hal::getDisplayInstance();
    if (!display) return;

    auto* gfx = static_cast<Gdey029T94*>(display->getNativeHandle());
    if (!gfx) return;

    if (!partial) {
        gfx->fillScreen(EPD_WHITE);
    }

    gfx->setTextColor(EPD_BLACK);

    // === Top Left: Clock ===
    gfx->setFont(nullptr);
    gfx->setTextSize(2);  // Size 2 for better fit
    gfx->setCursor(5, CLOCK_Y);
    gfx->print(clock_);

    // === Below clock: Date ===
    gfx->setTextSize(1);
    gfx->setCursor(5, DATE_Y);
    gfx->print(date_);

    // === Top Right: Battery ===
    renderBattery(gfx, BATTERY_X, BATTERY_Y);

    // === Right of battery: Status icons ===
    renderStatusIcons(gfx, BATTERY_X - 20, ICONS_Y);

    auto measure = [&](const char* t, const GFXfont* f, int16_t* x1, int16_t* y1, uint16_t* w, uint16_t* h) {
        if (f) cdc::ui::render::measureCp437Text(gfx, t, 0, 0, x1, y1, w, h);
        else gfx->getTextBounds(t, 0, 0, x1, y1, w, h);
    };
    auto draw = [&](const char* t, const GFXfont* f) {
        if (f) cdc::ui::render::drawCp437Text(gfx, t);
        else gfx->print(t);
    };

    // === Center: Name (size 3 = 12pt, fallback to smaller) ===
    if (name_[0]) {
        int16_t x1, y1;
        uint16_t w, h;

        // Try size 3 (12pt), then 2 (9pt), then 1 (built-in)
        int selectedSize = 3;
        for (int size = 3; size >= 1; size--) {
            const GFXfont* f = FONT_SIZES[size - 1];
            gfx->setFont(f);
            gfx->setTextSize(1);
            measure(name_, f, &x1, &y1, &w, &h);
            if (w < DISPLAY_WIDTH - 10) {
                selectedSize = size;
                break;
            }
        }

        const GFXfont* f = FONT_SIZES[selectedSize - 1];
        gfx->setFont(f);
        gfx->setTextSize(1);
        measure(name_, f, &x1, &y1, &w, &h);
        int nameX = (display->getWidth() - w) / 2;
        gfx->setCursor(nameX, NAME_Y);
        draw(name_, f);
    }

    // === Info line 1 (size 2 = 9pt, fallback to 1) ===
    if (info_[0]) {
        int16_t x1, y1;
        uint16_t w, h;

        // Try size 2 (9pt), then 1 (built-in)
        int selectedSize = 2;
        for (int size = 2; size >= 1; size--) {
            const GFXfont* f = FONT_SIZES[size - 1];
            gfx->setFont(f);
            gfx->setTextSize(1);
            measure(info_, f, &x1, &y1, &w, &h);
            if (w < DISPLAY_WIDTH - 10) {
                selectedSize = size;
                break;
            }
        }

        const GFXfont* f = FONT_SIZES[selectedSize - 1];
        gfx->setFont(f);
        gfx->setTextSize(1);
        measure(info_, f, &x1, &y1, &w, &h);
        int infoX = (display->getWidth() - w) / 2;
        gfx->setCursor(infoX, INFO_Y);
        draw(info_, f);
    }

    // === Info line 2 (size 2 = 9pt, fallback to 1) ===
    if (info2_[0]) {
        int16_t x1, y1;
        uint16_t w, h;

        // Try size 2 (9pt), then 1 (built-in)
        int selectedSize = 2;
        for (int size = 2; size >= 1; size--) {
            gfx->setFont(FONT_SIZES[size - 1]);
            gfx->setTextSize(1);
            gfx->getTextBounds(info2_, 0, 0, &x1, &y1, &w, &h);
            if (w < DISPLAY_WIDTH - 10) {
                selectedSize = size;
                break;
            }
        }

        gfx->setFont(FONT_SIZES[selectedSize - 1]);
        gfx->setTextSize(1);
        gfx->getTextBounds(info2_, 0, 0, &x1, &y1, &w, &h);
        int info2X = (display->getWidth() - w) / 2;
        gfx->setCursor(info2X, INFO2_Y);
        gfx->print(info2_);
    }

    // === Bottom: Footer hint (size 1 = built-in 6x8) ===
    gfx->setFont(nullptr);
    gfx->setTextSize(1);
    const char* hint = getFooterHint();
    if (hint && hint[0]) {
        int16_t x1, y1;
        uint16_t w, h;
        gfx->getTextBounds(hint, 0, 0, &x1, &y1, &w, &h);
        int hintX = (display->getWidth() - w) / 2;
        gfx->setCursor(hintX, display->getHeight() - 10);
        gfx->print(hint);
    }

    dirty_ = false;
}

} // namespace cdc::ui

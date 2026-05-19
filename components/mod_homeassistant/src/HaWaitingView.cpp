#include "mod_homeassistant/HaWaitingView.h"
#include "mod_homeassistant/HaClient.h"
#include "mod_homeassistant/HaStorage.h"
#include "mod_homeassistant/HaSetupWizardView.h"
#include "mod_homeassistant/HaHomeView.h"
#include "mod_homeassistant/HaI18n.h"
#include "cdc_hal/IDisplay.h"
#include "cdc_ui/ViewStack.h"
#include "cdc_views/ContextMenuView.h"
#include "cdc_log.h"
#include <goodisplay/gdey029T94.h>
#include <cstdio>

static const char* TAG = "HA_WAIT";

namespace cdc::mod_homeassistant {

static constexpr uint32_t POLL_INTERVAL_MS = 500;

/** \brief Context-menu items reused across the view's lifetime. */
static ui::ContextMenuView s_contextMenu;

void HaWaitingView::onEnter(void* context) {
    (void)context;
    urlPresent_   = HaClient::isUrlConfigured();
    tokenPresent_ = HaClient::isTokenConfigured();
    lastPollMs_   = 0;
    dirty_        = true;
    attemptTransition();
}

void HaWaitingView::onTick(uint32_t nowMs) {
    if (nowMs - lastPollMs_ < POLL_INTERVAL_MS) return;
    lastPollMs_ = nowMs;

    bool newUrl   = HaClient::isUrlConfigured();
    bool newToken = HaClient::isTokenConfigured();
    if (newUrl != urlPresent_ || newToken != tokenPresent_) {
        urlPresent_   = newUrl;
        tokenPresent_ = newToken;
        markDirty();
        attemptTransition();
    }
}

void HaWaitingView::attemptTransition() {
    if (urlPresent_ && tokenPresent_) {
        LOG_I(TAG, "Config complete, switching to HomeView");
        auto& home = HaHomeView::instance();
        home.init();
        ui::ViewStack::instance().push(&home);
    }
}

void HaWaitingView::render(bool partial) {
    auto* display = hal::getDisplayInstance();
    if (!display) return;
    auto* gfx = static_cast<Gdey029T94*>(display->getNativeHandle());
    if (!gfx) return;

    if (!partial) gfx->fillScreen(EPD_WHITE);

    gfx->setTextColor(EPD_BLACK);
    gfx->setTextSize(2);
    gfx->setCursor(8, 6);
    gfx->print(mstr(STR_SETUP_TITLE));
    gfx->drawFastHLine(0, 26, display->getWidth(), EPD_BLACK);

    gfx->setTextSize(1);
    int16_t y = 36;
    gfx->setCursor(8, y);
    gfx->print(mstr(urlPresent_   ? STR_URL_OK   : STR_URL_MISSING));
    y += 14;
    gfx->setCursor(8, y);
    gfx->print(mstr(tokenPresent_ ? STR_TOKEN_OK : STR_TOKEN_MISSING));

    y += 22;
    gfx->setCursor(8, y);
    gfx->print(mstr(STR_SERIAL_CMDS));
    y += 12;
    gfx->setCursor(8, y);
    gfx->print(" HA_URL <url>");
    y += 12;
    gfx->setCursor(8, y);
    gfx->print(" HA_TOKEN <token>");

    clearDirty();
}

/** \brief Anchor pointer captured for the wizard's popToAnchor. */
static ui::IView* s_wizardAnchor = nullptr;

/**
 * \brief Context menu action: open the GUI URL wizard.
 */
static void onMenuOpenWizard() {
    HaSetupWizardView::start(s_wizardAnchor);
}

void HaWaitingView::openContextMenu() {
    s_wizardAnchor = this;
    const ui::ContextMenuItem items[] = {
        {mstr(STR_ENTER_URL_GUI), onMenuOpenWizard},
    };
    s_contextMenu.init(mstr(STR_ACTIONS), items, 1);
    ui::ViewStack::instance().push(&s_contextMenu);
}

ui::InputResult HaWaitingView::onKey(char key) {
    switch (key) {
        case 'N':
            return ui::InputResult::REQUEST_POP;
        case '3':
            openContextMenu();
            return ui::InputResult::CONSUMED;
        default:
            return ui::InputResult::IGNORED;
    }
}

const char* HaWaitingView::getFooterHint() const {
    return mstr(STR_HINT_MENU_BACK);
}

} // namespace cdc::mod_homeassistant

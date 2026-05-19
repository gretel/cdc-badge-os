#include "mod_homeassistant/HaSetupWizardView.h"
#include "mod_homeassistant/HaStorage.h"
#include "mod_homeassistant/HaI18n.h"
#include "cdc_ui/ViewStack.h"
#include "cdc_views/T9InputView.h"
#include "cdc_views/ListView.h"
#include "cdc_views/ToastView.h"
#include "cdc_log.h"
#include <cstring>
#include <cstdio>

static const char* TAG = "HA_WIZ";

namespace cdc::mod_homeassistant {

/** \brief Wizard state captured between sequential steps. */
struct WizardState {
    char         host[64];
    char         port[8];
    bool         useHttps;
    ui::IView*   anchor;
};

static WizardState         s_state         = {};
static ui::T9InputView     s_t9Input;
static ui::ListView        s_yesNoMenu;

static void pushT9(const char* title, const char* initialText,
                   uint16_t maxLen, ui::T9InputView::SaveCallback cb) {
    s_t9Input.init(title, initialText, maxLen);
    s_t9Input.setOnSave(cb);
    ui::ViewStack::instance().push(&s_t9Input);
}

static void pushYesNoList(const char* title, ui::ListView::SelectCallback cb) {
    static const ui::ListItem items[2] = {
        {mstr(STR_NO),  0, false, nullptr},
        {mstr(STR_YES), 0, false, nullptr},
    };
    s_yesNoMenu.setOnSelect(cb);
    s_yesNoMenu.init(title, items, 2);
    ui::ViewStack::instance().push(&s_yesNoMenu);
}

void HaSetupWizardView::start(ui::IView* anchor) {
    memset(&s_state, 0, sizeof(s_state));
    strncpy(s_state.port, "8123", sizeof(s_state.port) - 1);
    s_state.anchor = anchor;
    pushT9(mstr(STR_HOST), nullptr, sizeof(s_state.host) - 1, onHostEntered);
}

void HaSetupWizardView::onHostEntered(const char* host) {
    if (!host || host[0] == '\0') {
        ui::showToastError(mstr(STR_HOST_REQUIRED));
        return;
    }
    strncpy(s_state.host, host, sizeof(s_state.host) - 1);
    pushT9(mstr(STR_PORT), s_state.port, sizeof(s_state.port) - 1, onPortEntered);
}

void HaSetupWizardView::onPortEntered(const char* port) {
    if (port && port[0] != '\0') {
        strncpy(s_state.port, port, sizeof(s_state.port) - 1);
    }
    pushYesNoList(mstr(STR_USE_HTTPS), onProtocolSelected);
}

void HaSetupWizardView::onProtocolSelected(uint16_t index, void* userData) {
    (void)userData;
    s_state.useHttps = (index == 1);
    finish();
}

void HaSetupWizardView::onSslSkipSelected(uint16_t index, void* userData) {
    (void)index;
    (void)userData;
    // The build never validates certificates; the prompt is retained in the
    // header only for binary compatibility with earlier wizard flows.
    finish();
}

void HaSetupWizardView::finish() {
    char url[256] = {};
    snprintf(url, sizeof(url), "%s://%s:%s",
             s_state.useHttps ? "https" : "http",
             s_state.host, s_state.port);

    if (!HaFavoriteStorage::writeUrl(url)) {
        LOG_E(TAG, "Failed to persist URL");
        ui::showToastError(mstr(STR_SAVE_FAILED));
        return;
    }
    LOG_I(TAG, "URL saved: %s", url);
    ui::showToastSuccess(mstr(STR_URL_SAVED));
    if (s_state.anchor) {
        ui::ViewStack::instance().popToAnchor(s_state.anchor);
    } else {
        for (int i = 0; i < 3; i++) ui::ViewStack::instance().pop();
    }
}

} // namespace cdc::mod_homeassistant

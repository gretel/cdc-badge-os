#include "mod_homeassistant/HaHomeView.h"
#include "mod_homeassistant/HaBrowseView.h"
#include "mod_homeassistant/HaLightDetailView.h"
#include "mod_homeassistant/HaStorage.h"
#include "mod_homeassistant/HaDomain.h"
#include "mod_homeassistant/HaI18n.h"
#include "cdc_os_ui/WifiHandlers.h"
#include "cdc_ui/I18n.h"
#include "cdc_ui/ViewStack.h"
#include "cdc_views/ContextMenuView.h"
#include "cdc_views/ConfirmView.h"
#include "cdc_views/ToastView.h"
#include "cdc_log.h"
#include <cstring>
#include <cstdio>

static const char* TAG = "HA_HOME";

namespace cdc::mod_homeassistant {

/** \brief Reused view instances (one per kind, shared across re-entries). */
static ui::ListView         s_listView;
static ui::ContextMenuView  s_contextMenu;
static HaBrowseView         s_browseView;
static HaLightDetailView    s_lightDetailView;

/** \brief Backing storage for the favorites list. */
static ui::ListItem s_listItems[HA_MAX_FAVORITES];
static char         s_listLabels[HA_MAX_FAVORITES][48];

/**
 * \brief Maps an entity state + favorite into a one-character status glyph.
 */
static const char* stateGlyph(const HaEntityState* e, const HaFavorite& fav) {
    HaDomain d = static_cast<HaDomain>(fav.domain);
    if (d == HaDomain::SCENE || d == HaDomain::SCRIPT || d == HaDomain::AUTOMATION) {
        return ">";
    }
    if (!e) return "?";
    switch (static_cast<HaState>(e->state)) {
        case HaState::ON:           return "*";
        case HaState::OFF:          return "o";
        case HaState::UNAVAILABLE:  return "!";
        default:                    return "?";
    }
}

HaHomeView& HaHomeView::instance() {
    static HaHomeView s_instance;
    return s_instance;
}

void HaHomeView::init() {
    HaFavoriteStorage::loadAll(favorites_);
    entityStates_.clear();
    rebuildListItems();
    s_listView.setOnSelect(cbSelect);
    s_listView.setOnMenu(cbMenu);
    s_listView.init(mstr(STR_TITLE), s_listItems,
                    static_cast<uint16_t>(favorites_.size()));
}

void HaHomeView::onEnter(void* context) {
    (void)context;
    auto& wifi = ui::WifiHandlers::instance();
    if (!wifi.ensureConnected()) {
        const char* err = wifi.getLastError();
        LOG_W(TAG, "WiFi ensureConnected failed: %s", err ? err : "(null)");
        ui::showToastError(err ? err : mstr(STR_NO_WIFI));
        return;
    }
    requestStates();
}

void HaHomeView::onExit() {
    // Releasing WiFi on exit pairs with onEnter()'s ensureConnected().
    ui::WifiHandlers::instance().disconnect();
}

void HaHomeView::onResume() {
    dirty_ = true;
    rebuildListItems();
    s_listView.init(mstr(STR_TITLE), s_listItems,
                    static_cast<uint16_t>(favorites_.size()));
}

void HaHomeView::rebuildListItems() {
    const size_t n = favorites_.size() > HA_MAX_FAVORITES
                         ? HA_MAX_FAVORITES
                         : favorites_.size();
    for (size_t i = 0; i < n; i++) {
        const HaFavorite& fav = favorites_[i];
        const HaEntityState* match = nullptr;
        for (const auto& e : entityStates_) {
            if (strcmp(e.entity_id, fav.entity_id) == 0) {
                match = &e;
                break;
            }
        }
        snprintf(s_listLabels[i], sizeof(s_listLabels[i]),
                 "%s %s", stateGlyph(match, fav), fav.display_name);
        s_listItems[i].label        = s_listLabels[i];
        s_listItems[i].icon         = 0;
        s_listItems[i].iconDisabled = false;
        s_listItems[i].userData     = nullptr;
    }
}

void HaHomeView::requestStates() {
    HaClient client;
    if (!client.loadConfig()) {
        ui::showToastError(mstr(STR_HA_CONFIG_MISSING));
        return;
    }
    HaResult res = client.getStates(entityStates_);
    if (res != HaResult::OK) {
        LOG_W(TAG, "getStates failed (res=%d, status=%d)",
              static_cast<int>(res), client.getLastHttpStatus());
        ui::showToastError(mstr(STR_HA_UNREACHABLE));
        return;
    }
    rebuildListItems();
    s_listView.init(mstr(STR_TITLE), s_listItems,
                    static_cast<uint16_t>(favorites_.size()));
}

void HaHomeView::cbSelect(uint16_t index, void* userData) {
    (void)userData;
    instance().handleSelect(index);
}

void HaHomeView::cbMenu(uint16_t index, void* userData) {
    (void)userData;
    instance().handleMenu(index);
}

void HaHomeView::handleSelect(uint16_t index) {
    if (index >= favorites_.size()) return;
    const HaFavorite& fav = favorites_[index];
    HaDomain d = static_cast<HaDomain>(fav.domain);
    const char* service = getPrimaryService(d);
    const char* domain  = getDomainString(d);
    if (!service || !domain) {
        ui::showToastError(mstr(STR_UNSUPPORTED));
        return;
    }

    HaClient client;
    if (!client.loadConfig()) {
        ui::showToastError(mstr(STR_HA_CONFIG_MISSING));
        return;
    }
    HaResult res = client.callService(domain, service, fav.entity_id);
    if (res != HaResult::OK) {
        LOG_W(TAG, "callService failed (res=%d, status=%d)",
              static_cast<int>(res), client.getLastHttpStatus());
        ui::showToastError(mstr(STR_HA_ERROR));
        return;
    }

    if (isToggleable(d)) {
        for (auto& e : entityStates_) {
            if (strcmp(e.entity_id, fav.entity_id) == 0) {
                e.state = (e.state == static_cast<uint8_t>(HaState::ON))
                              ? static_cast<uint8_t>(HaState::OFF)
                              : static_cast<uint8_t>(HaState::ON);
                break;
            }
        }
        rebuildListItems();
        s_listView.markDirty();
    }
}

void HaHomeView::handleMenu(uint16_t index) {
    if (index >= favorites_.size()) return;
    pendingMenuIndex_ = index;

    const HaFavorite& fav = favorites_[index];
    if (hasBrightness(static_cast<HaDomain>(fav.domain))) {
        const ui::ContextMenuItem items[] = {
            {mstr(STR_BRIGHTNESS),   cbBrightness},
            {mstr(STR_BROWSE_ALL),   cbBrowse},
            {mstr(STR_REMOVE),       cbRemove},
            {mstr(STR_RESET_MODULE), cbResetModule},
        };
        s_contextMenu.init(mstr(STR_ACTIONS), items, 4);
    } else {
        const ui::ContextMenuItem items[] = {
            {mstr(STR_BROWSE_ALL),   cbBrowse},
            {mstr(STR_REMOVE),       cbRemove},
            {mstr(STR_RESET_MODULE), cbResetModule},
        };
        s_contextMenu.init(mstr(STR_ACTIONS), items, 3);
    }
    ui::ViewStack::instance().push(&s_contextMenu);
}

void HaHomeView::cbBrightness() {
    auto& self = instance();
    if (self.pendingMenuIndex_ >= self.favorites_.size()) return;
    const HaFavorite& fav = self.favorites_[self.pendingMenuIndex_];
    if (!hasBrightness(static_cast<HaDomain>(fav.domain))) return;

    uint8_t initial = 50;
    for (const auto& e : self.entityStates_) {
        if (strcmp(e.entity_id, fav.entity_id) == 0) {
            initial = e.brightness > 0 ? e.brightness : 50;
            break;
        }
    }
    s_lightDetailView.init(fav.entity_id, fav.display_name, initial);
}

void HaHomeView::cbBrowse() {
    s_browseView.init(&instance().entityStates_);
    ui::ViewStack::instance().push(&s_browseView);
}

void HaHomeView::cbRemove() {
    auto& self = instance();
    if (self.pendingMenuIndex_ >= self.favorites_.size()) return;
    self.favorites_.erase(self.favorites_.begin() + self.pendingMenuIndex_);
    if (!HaFavoriteStorage::saveAll(self.favorites_)) {
        ui::showToastError(mstr(STR_SAVE_FAILED));
        return;
    }
    self.rebuildListItems();
    s_listView.init(mstr(STR_TITLE), s_listItems,
                    static_cast<uint16_t>(self.favorites_.size()));
    ui::showToastSuccess(mstr(STR_REMOVED));
}

void HaHomeView::cbResetModule() {
    ui::showConfirm(mstr(STR_RESET_CONFIRM),
                    []() {
                        HaClient::resetAll();
                        ui::showToastSuccess(mstr(STR_RESET_DONE));
                    });
}

void HaHomeView::render(bool) {
    clearDirty();
}

ui::InputResult HaHomeView::onKey(char) {
    // The underlying ListView is what the user actually interacts with; this
    // view exists only to own the favorites/state and to be pushed by the
    // module menu. All key handling lives on the ListView's callbacks.
    return ui::InputResult::IGNORED;
}

const char* HaHomeView::getFooterHint() const {
    return mstr(STR_HINT_TOGGLE_MENU);
}

} // namespace cdc::mod_homeassistant

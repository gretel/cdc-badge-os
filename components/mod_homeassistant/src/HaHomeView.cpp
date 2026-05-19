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
#include "cdc_views/RenderHelpers.h"
#include "cdc_log.h"
#include <cstring>
#include <cstdio>

static const char* TAG = "HA_HOME";

namespace cdc::mod_homeassistant {

/** \brief Reused view instances (one per kind, shared across re-entries). */
static ui::ListView         s_listView;
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
    s_listView.setHint(mstr(STR_HINT_TOGGLE_MENU));
    s_listView.setEmptyText(mstr(STR_NO_FAVORITES));
}

void HaHomeView::onEnter(void* context) {
    (void)context;

    // Render the HA view first (favorites with cached / stale state) so the
    // user sees the actual UI immediately. Connecting overlay comes on top.
    s_listView.markDirty();
    ui::ViewStack::instance().render();

    auto& wifi = ui::WifiHandlers::instance();
    bool needConnect = !wifi.isConnected();
    if (needConnect) {
        ui::showToastTask(ui::tr(ui::StringId::WIFI_CONNECTING), 0);
        ui::ViewStack::instance().render();
    }
    bool ok = wifi.ensureConnected();
    if (needConnect) {
        ui::ViewStack::instance().hideModal();
    }
    if (!ok) {
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
    HaFavoriteStorage::loadAll(favorites_);
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
        ui::render::utf8ToCp437Inplace(s_listLabels[i]);
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

    // Read-only entities (sensor, binary_sensor): show current state instead of
    // attempting a service call that would 500.
    if (isReadOnly(d)) {
        const HaEntityState* match = nullptr;
        for (const auto& e : entityStates_) {
            if (strcmp(e.entity_id, fav.entity_id) == 0) { match = &e; break; }
        }
        if (!match) {
            ui::showToastError(mstr(STR_HA_UNREACHABLE));
            return;
        }
        char msg[96];
        const char* stateStr = (match->state == static_cast<uint8_t>(HaState::ON))   ? "on"
                              : (match->state == static_cast<uint8_t>(HaState::OFF)) ? "off"
                              : "?";
        snprintf(msg, sizeof(msg), "%s\n%s", fav.display_name, stateStr);
        ui::showToastInfo(msg, 2500);
        return;
    }

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
    pendingMenuIndex_ = index;

    // Empty favorites list: only the browse action is meaningful.
    if (index >= favorites_.size()) {
        const ui::ContextMenuItem items[] = {
            {mstr(STR_BROWSE_ALL), cbBrowse},
        };
        ui::showContextMenu(mstr(STR_ACTIONS), items, 1);
        return;
    }

    const HaFavorite& fav = favorites_[index];
    if (hasBrightness(static_cast<HaDomain>(fav.domain))) {
        const ui::ContextMenuItem items[] = {
            {mstr(STR_BRIGHTNESS), cbBrightness},
            {mstr(STR_BROWSE_ALL), cbBrowse},
            {mstr(STR_REMOVE),     cbRemove},
        };
        ui::showContextMenu(mstr(STR_ACTIONS), items, 3);
    } else {
        const ui::ContextMenuItem items[] = {
            {mstr(STR_BROWSE_ALL), cbBrowse},
            {mstr(STR_REMOVE),     cbRemove},
        };
        ui::showContextMenu(mstr(STR_ACTIONS), items, 2);
    }
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
    HaBrowseView::instance().init(&instance().entityStates_);
    ui::ViewStack::instance().push(&HaBrowseView::instance());
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
                    [](void*) {
                        HaClient::resetAll();
                        ui::showToastSuccess(mstr(STR_RESET_DONE));
                    });
}

void HaHomeView::render(bool partial) {
    s_listView.render(partial);
    clearDirty();
}

ui::InputResult HaHomeView::onKey(char key) {
    return s_listView.onKey(key);
}

ui::InputResult HaHomeView::onLongPress(char key) {
    return s_listView.onLongPress(key);
}

void HaHomeView::onTick(uint32_t nowMs) {
    s_listView.onTick(nowMs);
}

bool HaHomeView::needsRender() const {
    return s_listView.needsRender();
}

const char* HaHomeView::getFooterHint() const {
    return mstr(STR_HINT_TOGGLE_MENU);
}

} // namespace cdc::mod_homeassistant

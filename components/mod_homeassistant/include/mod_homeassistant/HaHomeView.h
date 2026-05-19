#pragma once

#include "cdc_ui/IView.h"
#include "cdc_views/ListView.h"
#include "mod_homeassistant/HaClient.h"
#include "mod_homeassistant/HaFavorite.h"
#include <vector>

namespace cdc::mod_homeassistant {

/**
 * \brief Module main view: favorites list with state markers.
 *
 * Singleton (`instance()`); the module-level views (`HaWaitingView`, menu
 * factory in `HomeAssistantModule`) push the same instance so the view's
 * favorites + entity cache survive across re-entries.
 *
 * Lifecycle:
 *  - `onEnter`: calls `WifiHandlers::ensureConnected()`, then fetches states.
 *  - `onExit`:  releases WiFi (`WifiHandlers::disconnect()`).
 *  - Child views (Browse, LightDetail) push on top and do not manage WiFi.
 *
 * Keys (delegated to the underlying `ListView`):
 *  - `Y` toggles/triggers the selected favorite.
 *  - `3` opens the context menu (Brightness… for lights, Browse, Remove, Reset).
 *  - `N` pops back to the main menu.
 */
class HaHomeView : public ui::ViewBase {
public:
    /**
     * \brief Returns the single shared instance of this view.
     */
    static HaHomeView& instance();

    /**
     * \brief Loads favorites from NVS and configures the inner ListView.
     *        Must be called before the view is pushed.
     */
    void init();

    void onEnter(void* context) override;
    void onExit() override;
    void onResume() override;
    void render(bool partial) override;
    ui::InputResult onKey(char key) override;
    const char* getName() const override { return "HaHomeView"; }
    const char* getFooterHint() const override;

private:
    HaHomeView() = default;

    // ListView callbacks (registered via setOnSelect / setOnMenu).
    static void cbSelect(uint16_t index, void* userData);
    static void cbMenu  (uint16_t index, void* userData);

    // Context-menu actions (registered as ContextMenuItem callbacks).
    static void cbBrightness();
    static void cbBrowse();
    static void cbRemove();
    static void cbResetModule();

    void handleSelect(uint16_t index);
    void handleMenu  (uint16_t index);
    void rebuildListItems();
    void requestStates();

    std::vector<HaFavorite>     favorites_;
    std::vector<HaEntityState>  entityStates_;
    uint16_t                    pendingMenuIndex_ = 0;
};

} // namespace cdc::mod_homeassistant

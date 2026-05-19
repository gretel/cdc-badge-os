#pragma once

#include "cdc_ui/IView.h"
#include "mod_homeassistant/HaFavorite.h"
#include <vector>

namespace cdc::mod_homeassistant {

/**
 * \brief Browse view: lists all supported entities to pick favorites from.
 *
 * Pulled from a cached `HaEntityState` vector (typically owned by `HaHomeView`).
 * Singleton via `instance()`. Entries are filtered to the supported domains
 * and sorted alphabetically.
 *
 * Keys (via the underlying `ListView`):
 *  - `Y` adds the selected entity to the favorites list.
 *  - `3` opens a context menu with "Search…" and per-domain filters.
 *  - `N` returns to the HomeView.
 */
class HaBrowseView : public ui::ViewBase {
public:
    static HaBrowseView& instance();

    /**
     * \brief Configures the source entity list before pushing the view.
     */
    void init(const std::vector<HaEntityState>* sourceEntities);

    void onEnter(void* context) override;
    void render(bool partial) override;
    ui::InputResult onKey(char key) override;
    const char* getName() const override { return "HaBrowseView"; }
    const char* getFooterHint() const override;

private:
    HaBrowseView() = default;

    // ListView + ContextMenu callbacks.
    static void cbSelect(uint16_t index, void* userData);
    static void cbMenu  (uint16_t index, void* userData);
    static void cbFilterAll();
    static void cbFilterLight();
    static void cbFilterSwitch();
    static void cbFilterScene();
    static void cbSearch();
    static void cbSearchSaved(const char* text);

    void handleSelect(uint16_t index);
    void handleMenu  (uint16_t index);
    void applySearch(const char* term);
    void applyFilter(uint8_t domainFilter);
    void rebuildList();
    static bool matchesFilter(const HaEntityState& e,
                              const char* searchTerm,
                              uint8_t domainFilter);

    const std::vector<HaEntityState>* source_ = nullptr;
    char    searchTerm_[32]   = {};
    uint8_t domainFilter_     = 0xFF;
};

} // namespace cdc::mod_homeassistant

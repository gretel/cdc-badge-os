#include "mod_homeassistant/HaBrowseView.h"
#include "mod_homeassistant/HaDomain.h"
#include "mod_homeassistant/HaStorage.h"
#include "mod_homeassistant/HaI18n.h"
#include "cdc_ui/ViewStack.h"
#include "cdc_views/ListView.h"
#include "cdc_views/ContextMenuView.h"
#include "cdc_views/T9InputView.h"
#include "cdc_views/ToastView.h"
#include "cdc_log.h"
#include <cctype>
#include <cstring>

static const char* TAG = "HA_BROWSE";

namespace cdc::mod_homeassistant {

static constexpr uint16_t MAX_BROWSE_ROWS = 256;

/** \brief Shared view instances reused across pushes. */
static ui::ListView          s_listView;
static ui::T9InputView       s_searchInput;
static ui::ContextMenuView   s_contextMenu;

/** \brief Backing storage for the rendered list. */
static ui::ListItem          s_listItems[MAX_BROWSE_ROWS];
static char                  s_listLabels[MAX_BROWSE_ROWS][64];
static const HaEntityState*  s_listEntities[MAX_BROWSE_ROWS];
static uint16_t              s_listCount = 0;

HaBrowseView& HaBrowseView::instance() {
    static HaBrowseView s_instance;
    return s_instance;
}

bool HaBrowseView::matchesFilter(const HaEntityState& e,
                                 const char* searchTerm,
                                 uint8_t domainFilter) {
    if (domainFilter != 0xFF && e.domain != domainFilter) return false;
    if (!searchTerm || searchTerm[0] == '\0') return true;

    auto lower = [](char c) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    };
    const char* haystack = e.friendly_name[0] ? e.friendly_name : e.entity_id;
    size_t hLen = strlen(haystack);
    size_t nLen = strlen(searchTerm);
    if (nLen > hLen) return false;
    for (size_t i = 0; i + nLen <= hLen; i++) {
        size_t j = 0;
        for (; j < nLen; j++) {
            if (lower(haystack[i + j]) != lower(searchTerm[j])) break;
        }
        if (j == nLen) return true;
    }
    return false;
}

void HaBrowseView::init(const std::vector<HaEntityState>* sourceEntities) {
    source_         = sourceEntities;
    searchTerm_[0]  = '\0';
    domainFilter_   = 0xFF;
}

void HaBrowseView::onEnter(void* context) {
    (void)context;
    rebuildList();
}

void HaBrowseView::rebuildList() {
    s_listCount = 0;
    if (source_) {
        for (const auto& e : *source_) {
            if (s_listCount >= MAX_BROWSE_ROWS) break;
            if (!matchesFilter(e, searchTerm_, domainFilter_)) continue;
            const char* lbl = e.friendly_name[0] ? e.friendly_name : e.entity_id;
            strncpy(s_listLabels[s_listCount], lbl, sizeof(s_listLabels[0]) - 1);
            s_listLabels[s_listCount][sizeof(s_listLabels[0]) - 1] = '\0';
            s_listItems[s_listCount].label        = s_listLabels[s_listCount];
            s_listItems[s_listCount].icon         = 0;
            s_listItems[s_listCount].iconDisabled = false;
            s_listItems[s_listCount].userData     = nullptr;
            s_listEntities[s_listCount]           = &e;
            s_listCount++;
        }
    }

    s_listView.setOnSelect(cbSelect);
    s_listView.setOnMenu(cbMenu);
    s_listView.init(mstr(STR_BROWSE), s_listItems, s_listCount);
}

void HaBrowseView::cbSelect(uint16_t index, void* userData) {
    (void)userData;
    instance().handleSelect(index);
}

void HaBrowseView::cbMenu(uint16_t index, void* userData) {
    (void)userData;
    instance().handleMenu(index);
}

void HaBrowseView::handleSelect(uint16_t index) {
    if (index >= s_listCount) return;
    const HaEntityState* e = s_listEntities[index];
    if (!e) return;

    std::vector<HaFavorite> favs;
    HaFavoriteStorage::loadAll(favs);
    if (favs.size() >= HA_MAX_FAVORITES) {
        ui::showToastError(mstr(STR_MAX_FAVS));
        return;
    }

    HaFavorite fav = {};
    strncpy(fav.entity_id,    e->entity_id,    sizeof(fav.entity_id)    - 1);
    strncpy(fav.display_name, e->friendly_name[0] ? e->friendly_name : e->entity_id,
            sizeof(fav.display_name) - 1);
    fav.domain = e->domain;
    fav.flags  = 0;

    favs.push_back(fav);
    if (!HaFavoriteStorage::saveAll(favs)) {
        ui::showToastError(mstr(STR_SAVE_FAILED));
        return;
    }
    LOG_I(TAG, "Added favorite: %s", fav.entity_id);
    ui::showToastSuccess(mstr(STR_ADDED));
}

void HaBrowseView::handleMenu(uint16_t index) {
    (void)index;
    const ui::ContextMenuItem items[] = {
        {mstr(STR_SEARCH),      cbSearch},
        {mstr(STR_ALL_DOMAINS), cbFilterAll},
        {mstr(STR_LIGHTS),      cbFilterLight},
        {mstr(STR_SWITCHES),    cbFilterSwitch},
        {mstr(STR_SCENES),      cbFilterScene},
    };
    s_contextMenu.init(mstr(STR_FILTER), items, 5);
    ui::ViewStack::instance().push(&s_contextMenu);
}

void HaBrowseView::cbSearch() {
    s_searchInput.init(mstr(STR_SEARCH), instance().searchTerm_, 31);
    s_searchInput.setOnSave(cbSearchSaved);
    ui::ViewStack::instance().push(&s_searchInput);
}

void HaBrowseView::cbSearchSaved(const char* text) {
    instance().applySearch(text);
}

void HaBrowseView::cbFilterAll()    { instance().applyFilter(0xFF); }
void HaBrowseView::cbFilterLight()  { instance().applyFilter(static_cast<uint8_t>(HaDomain::LIGHT)); }
void HaBrowseView::cbFilterSwitch() { instance().applyFilter(static_cast<uint8_t>(HaDomain::SWITCH)); }
void HaBrowseView::cbFilterScene()  { instance().applyFilter(static_cast<uint8_t>(HaDomain::SCENE)); }

void HaBrowseView::applySearch(const char* term) {
    searchTerm_[0] = '\0';
    if (term) {
        strncpy(searchTerm_, term, sizeof(searchTerm_) - 1);
        searchTerm_[sizeof(searchTerm_) - 1] = '\0';
    }
    rebuildList();
    s_listView.markDirty();
}

void HaBrowseView::applyFilter(uint8_t domainFilter) {
    domainFilter_ = domainFilter;
    rebuildList();
    s_listView.markDirty();
}

void HaBrowseView::render(bool) {
    clearDirty();
}

ui::InputResult HaBrowseView::onKey(char) {
    // All real input is consumed by the inner ListView via its callbacks.
    return ui::InputResult::IGNORED;
}

const char* HaBrowseView::getFooterHint() const {
    return mstr(STR_HINT_ADD_FILTER);
}

} // namespace cdc::mod_homeassistant

#include "mod_homeassistant/HomeAssistantModule.h"
#include "mod_homeassistant/HaClient.h"
#include "mod_homeassistant/HaHomeView.h"
#include "mod_homeassistant/HaWaitingView.h"
#include "mod_homeassistant/HaI18n.h"
#include "cdc_core/ModuleRegistry.h"
#include "cdc_ui/I18n.h"
#include "cdc_ui/ViewStack.h"
#include "cdc_log.h"

static const char* TAG = "HA";

namespace cdc::mod_homeassistant {

/** \brief Module-specific i18n string offsets. */
static uint16_t s_strIdBase = 0;

/** \brief Top-level setup-pending view (HomeView is a singleton elsewhere). */
static HaWaitingView s_waitingView;

extern void registerHaSerialCommands();

const char* mstr(uint16_t offset) {
    return ui::tr(s_strIdBase + offset);
}

void registerStrings() {
    auto& i18n = ui::I18n::instance();
    s_strIdBase = i18n.registerModule("mod_homeassistant", STR_COUNT);
    if (s_strIdBase == 0) {
        LOG_E(TAG, "Failed to register i18n strings");
        return;
    }

    // English
    i18n.registerTranslation(s_strIdBase + STR_TITLE,             ui::Language::EN, "Home Assistant");
    i18n.registerTranslation(s_strIdBase + STR_SETUP_TITLE,       ui::Language::EN, "HA Setup");
    i18n.registerTranslation(s_strIdBase + STR_URL_OK,            ui::Language::EN, "URL:   OK");
    i18n.registerTranslation(s_strIdBase + STR_URL_MISSING,       ui::Language::EN, "URL:   missing");
    i18n.registerTranslation(s_strIdBase + STR_TOKEN_OK,          ui::Language::EN, "Token: OK");
    i18n.registerTranslation(s_strIdBase + STR_TOKEN_MISSING,     ui::Language::EN, "Token: missing");
    i18n.registerTranslation(s_strIdBase + STR_SERIAL_CMDS,       ui::Language::EN, "Serial commands:");
    i18n.registerTranslation(s_strIdBase + STR_ENTER_URL_GUI,     ui::Language::EN, "Enter URL (GUI)");
    i18n.registerTranslation(s_strIdBase + STR_ACTIONS,           ui::Language::EN, "Actions");
    i18n.registerTranslation(s_strIdBase + STR_HINT_MENU_BACK,    ui::Language::EN, "[3] Menu  [N] Back");
    i18n.registerTranslation(s_strIdBase + STR_HINT_ADD_FILTER,   ui::Language::EN, "[Y] Add  [3] Filter  [N] Back");
    i18n.registerTranslation(s_strIdBase + STR_HINT_TOGGLE_MENU,  ui::Language::EN, "[Y] Toggle  [3] Menu  [N] Back");
    i18n.registerTranslation(s_strIdBase + STR_HOST,              ui::Language::EN, "HA Host");
    i18n.registerTranslation(s_strIdBase + STR_PORT,              ui::Language::EN, "HA Port");
    i18n.registerTranslation(s_strIdBase + STR_USE_HTTPS,         ui::Language::EN, "Use HTTPS?");
    i18n.registerTranslation(s_strIdBase + STR_SKIP_CERT_CHECK,   ui::Language::EN, "Skip cert check?");
    i18n.registerTranslation(s_strIdBase + STR_YES,               ui::Language::EN, "Yes");
    i18n.registerTranslation(s_strIdBase + STR_NO,                ui::Language::EN, "No");
    i18n.registerTranslation(s_strIdBase + STR_URL_SAVED,         ui::Language::EN, "URL saved");
    i18n.registerTranslation(s_strIdBase + STR_SAVE_FAILED,       ui::Language::EN, "Save failed");
    i18n.registerTranslation(s_strIdBase + STR_HOST_REQUIRED,     ui::Language::EN, "Host required");
    i18n.registerTranslation(s_strIdBase + STR_SEARCH,            ui::Language::EN, "Search...");
    i18n.registerTranslation(s_strIdBase + STR_ALL_DOMAINS,       ui::Language::EN, "All domains");
    i18n.registerTranslation(s_strIdBase + STR_LIGHTS,            ui::Language::EN, "Lights");
    i18n.registerTranslation(s_strIdBase + STR_SWITCHES,          ui::Language::EN, "Switches");
    i18n.registerTranslation(s_strIdBase + STR_SCENES,            ui::Language::EN, "Scenes");
    i18n.registerTranslation(s_strIdBase + STR_FILTER,            ui::Language::EN, "Filter");
    i18n.registerTranslation(s_strIdBase + STR_BROWSE,            ui::Language::EN, "Browse");
    i18n.registerTranslation(s_strIdBase + STR_BROWSE_ALL,        ui::Language::EN, "Browse all");
    i18n.registerTranslation(s_strIdBase + STR_BRIGHTNESS,        ui::Language::EN, "Brightness...");
    i18n.registerTranslation(s_strIdBase + STR_REMOVE,            ui::Language::EN, "Remove");
    i18n.registerTranslation(s_strIdBase + STR_RESET_MODULE,      ui::Language::EN, "Reset module");
    i18n.registerTranslation(s_strIdBase + STR_RESET_CONFIRM,     ui::Language::EN, "Reset all HA data?");
    i18n.registerTranslation(s_strIdBase + STR_RESET_DONE,        ui::Language::EN, "Reset done");
    i18n.registerTranslation(s_strIdBase + STR_MAX_FAVS,          ui::Language::EN, "Max favorites reached");
    i18n.registerTranslation(s_strIdBase + STR_ADDED,             ui::Language::EN, "Added");
    i18n.registerTranslation(s_strIdBase + STR_REMOVED,           ui::Language::EN, "Removed");
    i18n.registerTranslation(s_strIdBase + STR_TOAST_OK,          ui::Language::EN, "OK");
    i18n.registerTranslation(s_strIdBase + STR_HA_ERROR,          ui::Language::EN, "HA error");
    i18n.registerTranslation(s_strIdBase + STR_HA_UNREACHABLE,    ui::Language::EN, "HA unreachable");
    i18n.registerTranslation(s_strIdBase + STR_HA_CONFIG_MISSING, ui::Language::EN, "HA config missing");
    i18n.registerTranslation(s_strIdBase + STR_NO_WIFI,           ui::Language::EN, "No WiFi");
    i18n.registerTranslation(s_strIdBase + STR_UNSUPPORTED,       ui::Language::EN, "Unsupported");
    i18n.registerTranslation(s_strIdBase + STR_NO_FAVORITES,      ui::Language::EN, "No favorites - press [3] to browse");

    // German
    i18n.registerTranslation(s_strIdBase + STR_TITLE,             ui::Language::DE, "Home Assistant");
    i18n.registerTranslation(s_strIdBase + STR_SETUP_TITLE,       ui::Language::DE, "HA Einrichtung");
    i18n.registerTranslation(s_strIdBase + STR_URL_OK,            ui::Language::DE, "URL:   OK");
    i18n.registerTranslation(s_strIdBase + STR_URL_MISSING,       ui::Language::DE, "URL:   fehlt");
    i18n.registerTranslation(s_strIdBase + STR_TOKEN_OK,          ui::Language::DE, "Token: OK");
    i18n.registerTranslation(s_strIdBase + STR_TOKEN_MISSING,     ui::Language::DE, "Token: fehlt");
    i18n.registerTranslation(s_strIdBase + STR_SERIAL_CMDS,       ui::Language::DE, "Serielle Befehle:");
    i18n.registerTranslation(s_strIdBase + STR_ENTER_URL_GUI,     ui::Language::DE, "URL eingeben (GUI)");
    i18n.registerTranslation(s_strIdBase + STR_ACTIONS,           ui::Language::DE, "Aktionen");
    i18n.registerTranslation(s_strIdBase + STR_HINT_MENU_BACK,    ui::Language::DE, "[3] Menue  [N] Zurueck");
    i18n.registerTranslation(s_strIdBase + STR_HINT_ADD_FILTER,   ui::Language::DE, "[Y] Hinzu  [3] Filter  [N] Zurueck");
    i18n.registerTranslation(s_strIdBase + STR_HINT_TOGGLE_MENU,  ui::Language::DE, "[Y] Schalten  [3] Menue  [N] Zurueck");
    i18n.registerTranslation(s_strIdBase + STR_HOST,              ui::Language::DE, "HA Host");
    i18n.registerTranslation(s_strIdBase + STR_PORT,              ui::Language::DE, "HA Port");
    i18n.registerTranslation(s_strIdBase + STR_USE_HTTPS,         ui::Language::DE, "HTTPS verwenden?");
    i18n.registerTranslation(s_strIdBase + STR_SKIP_CERT_CHECK,   ui::Language::DE, "Zertifikatspruefung ueberspringen?");
    i18n.registerTranslation(s_strIdBase + STR_YES,               ui::Language::DE, "Ja");
    i18n.registerTranslation(s_strIdBase + STR_NO,                ui::Language::DE, "Nein");
    i18n.registerTranslation(s_strIdBase + STR_URL_SAVED,         ui::Language::DE, "URL gespeichert");
    i18n.registerTranslation(s_strIdBase + STR_SAVE_FAILED,       ui::Language::DE, "Speichern fehlgeschlagen");
    i18n.registerTranslation(s_strIdBase + STR_HOST_REQUIRED,     ui::Language::DE, "Host erforderlich");
    i18n.registerTranslation(s_strIdBase + STR_SEARCH,            ui::Language::DE, "Suchen...");
    i18n.registerTranslation(s_strIdBase + STR_ALL_DOMAINS,       ui::Language::DE, "Alle");
    i18n.registerTranslation(s_strIdBase + STR_LIGHTS,            ui::Language::DE, "Lichter");
    i18n.registerTranslation(s_strIdBase + STR_SWITCHES,          ui::Language::DE, "Schalter");
    i18n.registerTranslation(s_strIdBase + STR_SCENES,            ui::Language::DE, "Szenen");
    i18n.registerTranslation(s_strIdBase + STR_FILTER,            ui::Language::DE, "Filter");
    i18n.registerTranslation(s_strIdBase + STR_BROWSE,            ui::Language::DE, "Durchsuchen");
    i18n.registerTranslation(s_strIdBase + STR_BROWSE_ALL,        ui::Language::DE, "Alle anzeigen");
    i18n.registerTranslation(s_strIdBase + STR_BRIGHTNESS,        ui::Language::DE, "Helligkeit...");
    i18n.registerTranslation(s_strIdBase + STR_REMOVE,            ui::Language::DE, "Entfernen");
    i18n.registerTranslation(s_strIdBase + STR_RESET_MODULE,      ui::Language::DE, "Modul zuruecksetzen");
    i18n.registerTranslation(s_strIdBase + STR_RESET_CONFIRM,     ui::Language::DE, "Alle HA-Daten loeschen?");
    i18n.registerTranslation(s_strIdBase + STR_RESET_DONE,        ui::Language::DE, "Zuruecksetzen erledigt");
    i18n.registerTranslation(s_strIdBase + STR_MAX_FAVS,          ui::Language::DE, "Max. Favoriten erreicht");
    i18n.registerTranslation(s_strIdBase + STR_ADDED,             ui::Language::DE, "Hinzugefuegt");
    i18n.registerTranslation(s_strIdBase + STR_REMOVED,           ui::Language::DE, "Entfernt");
    i18n.registerTranslation(s_strIdBase + STR_TOAST_OK,          ui::Language::DE, "OK");
    i18n.registerTranslation(s_strIdBase + STR_HA_ERROR,          ui::Language::DE, "HA Fehler");
    i18n.registerTranslation(s_strIdBase + STR_HA_UNREACHABLE,    ui::Language::DE, "HA nicht erreichbar");
    i18n.registerTranslation(s_strIdBase + STR_HA_CONFIG_MISSING, ui::Language::DE, "HA Konfiguration fehlt");
    i18n.registerTranslation(s_strIdBase + STR_NO_WIFI,           ui::Language::DE, "Kein WLAN");
    i18n.registerTranslation(s_strIdBase + STR_UNSUPPORTED,       ui::Language::DE, "Nicht unterstuetzt");
    i18n.registerTranslation(s_strIdBase + STR_NO_FAVORITES,      ui::Language::DE, "Keine Favoriten - [3] fuer Browse");

    LOG_I(TAG, "Registered i18n strings (base=%d)", s_strIdBase);
}

HomeAssistantModule& HomeAssistantModule::instance() {
    static HomeAssistantModule inst;
    return inst;
}

bool HomeAssistantModule::init() {
    LOG_I(TAG, "Initializing Home Assistant module");
    registerStrings();
    registerHaSerialCommands();
    core::ModuleRegistry::instance().registerModule(this);
    if (!slotRange_.hasRmem) {
        core::ModuleRegistry::instance().reportModuleError(getName(),
                                                           "HA slot range missing");
        state_ = core::ServiceState::ERROR;
        return false;
    }
    state_ = core::ServiceState::INITIALIZED;
    return true;
}

void HomeAssistantModule::stop() {
    ModuleBase::stop();
}

void HomeAssistantModule::setSlotRange(const core::IModule::SlotRange& range) {
    slotRange_ = range;
}

core::IModule::SlotRequest HomeAssistantModule::getSlotRequest() const {
    core::IModule::SlotRequest req = {};
    req.mapName      = "mod_homeassistant";
    req.minEccSlots  = 0;
    req.minRmemSlots = 1;
    return req;
}

uint8_t HomeAssistantModule::getMenuItems(core::ModuleMenuItem* items,
                                          uint8_t maxItems) {
    if (!items || maxItems == 0) return 0;
    items[0] = {
        mstr(STR_TITLE),
        80,                                 // priority
        []() -> ui::IView* {
            if (!HaClient::isUrlConfigured() || !HaClient::isTokenConfigured()) {
                return &s_waitingView;
            }
            auto& home = HaHomeView::instance();
            home.init();
            return &home;
        },
        nullptr,
        "mod_homeassistant",
        core::MenuLocation::MAIN_MENU,
        nullptr
    };
    return 1;
}

} // namespace cdc::mod_homeassistant

extern "C" void mod_homeassistant_register() {
    cdc::core::ModuleRegistry::instance().registerInitializer([]() {
        auto& module = cdc::mod_homeassistant::HomeAssistantModule::instance();
        if (module.init()) {
            module.start();
        }
    });
}

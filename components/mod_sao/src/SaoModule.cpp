#include "mod_sao/SaoModule.h"
#include "mod_sao/sao.h"
#include "cdc_core/ModuleRegistry.h"
#include "cdc_ui/I18n.h"
#include "cdc_views/InfoView.h"
#include "cdc_log.h"
#include <cstring>

static const char* TAG = "SAO";

namespace cdc::mod_sao {

static uint16_t s_strIdBase = 0;
static constexpr uint16_t STR_SAO = 0;
static constexpr uint16_t STR_COUNT = 1;

static const char* mstr(uint16_t offset) {
    return ui::tr(s_strIdBase + offset);
}

static void registerStrings() {
    auto& i18n = ui::I18n::instance();
    s_strIdBase = i18n.registerModule("mod_sao", STR_COUNT);
    if (s_strIdBase == 0) {
        LOG_E(TAG, "Failed to register i18n strings");
        return;
    }
    i18n.registerTranslation(s_strIdBase + STR_SAO, ui::Language::EN, "SAO");
    i18n.registerTranslation(s_strIdBase + STR_SAO, ui::Language::DE, "SAO");
}

static ui::InfoView s_infoView;
static char s_infoText[256];

static ui::IView* getInfoView() {
    sao_get_info_string(s_infoText, sizeof(s_infoText));
    s_infoView.init(mstr(STR_SAO), s_infoText);
    return &s_infoView;
}

SaoModule& SaoModule::instance() {
    static SaoModule inst;
    return inst;
}

bool SaoModule::init() {
    LOG_I(TAG, "Initializing SAO module");
    registerStrings();
    core::ModuleRegistry::instance().registerModule(this);
    state_ = core::ServiceState::INITIALIZED;
    return true;
}

bool SaoModule::start() {
    if (state_ != core::ServiceState::INITIALIZED &&
        state_ != core::ServiceState::STOPPED) {
        return false;
    }
    if (!sao_init()) {
        LOG_W(TAG, "SAO init failed (I2C1 may be unavailable)");
    }
    state_ = core::ServiceState::STARTED;
    return true;
}

void SaoModule::stop() {
    state_ = core::ServiceState::STOPPED;
}

uint8_t SaoModule::getMenuItems(core::ModuleMenuItem* items, uint8_t maxItems) {
    if (!items || maxItems == 0) return 0;
    items[0] = {
        mstr(STR_SAO),
        120,
        getInfoView,
        nullptr,
        getName(),
        core::MenuLocation::TOOLS_MENU,
        nullptr
    };
    return 1;
}

void SaoModule::onUnlock() {
    if (state_ != core::ServiceState::STARTED) return;
    sao_scan();
}

} // namespace cdc::mod_sao

extern "C" void mod_sao_register() {
    cdc::core::ModuleRegistry::instance().registerInitializer([]() {
        auto& module = cdc::mod_sao::SaoModule::instance();
        if (module.init()) {
            module.start();
        }
    });
}

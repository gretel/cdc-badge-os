#include "cdc_os_ui/AppUi.h"

#include "cdc_ui/ViewStack.h"
#include "cdc_ui/I18n.h"
#include "cdc_os_ui/views/LockScreenView.h"
#include "cdc_os_ui/views/PinChangeView.h"
#include "cdc_os_ui/views/WifiListView.h"
#include "cdc_os_ui/WifiHandlers.h"
#include "cdc_os_ui/SettingsHandlers.h"
#include "cdc_os_ui/SleepManager.h"
#include "cdc_os_ui/HardwareInfo.h"
#include "cdc_core/PinManager.h"
#include "cdc_core/ModuleRegistry.h"
#include "cdc_core/EventBus.h"
#include "cdc_core/TropicSlotMap.h"
#include "cdc_core/TropicStorage.h"
#include "cdc_core/UsbManager.h"

#include "cdc_views/ListView.h"
#include "cdc_views/SliderView.h"
#include "cdc_views/PinEntryView.h"
#include "cdc_views/T9InputView.h"
#include "cdc_views/DateInputView.h"
#include "cdc_views/TimeInputView.h"
#include "cdc_views/InfoView.h"
#include "cdc_views/ToastView.h"
#include "cdc_views/ConfirmView.h"

#include "cdc_hal/IDisplay.h"
#include "cdc_hal/IKeypad.h"
#include "cdc_hal/IPowerManager.h"
#include "cdc_hal/ISleepController.h"
#include "cdc_hal/ISecureElement.h"
#include "cdc_hal/IWifiController.h"
#include "cdc_hal/IBluetoothController.h"
#include "cdc_hal/IRtc.h"

#include "serial_cmd/SerialCmd.h"
#include "nvs.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

namespace cdc::ui {

// ============================================================================
// Constants
// ============================================================================

// Menu limits
static constexpr uint8_t MAIN_MENU_MAX_ITEMS = 16;
static constexpr uint8_t MAIN_MENU_FIXED_COUNT = 2;  // Tools + Settings
static constexpr uint8_t TOOLS_FIXED_COUNT = 4;      // Modules, WiFi, Bluetooth, Expert
static constexpr uint8_t TOOLS_MAX_ITEMS = 16;
static constexpr uint8_t EXPERT_COUNT = 3;
static constexpr uint8_t MODULES_VIEW_MAX = 16;
static constexpr uint8_t WIFI_MAX_NETWORKS = 20;

// Inactivity timeout (5 minutes)
static constexpr uint32_t INACTIVITY_TIMEOUT_MS = 5 * 60 * 1000;

// Toast duration constants (milliseconds)
static constexpr uint32_t TOAST_DURATION_SHORT_MS = 1000;
static constexpr uint32_t TOAST_DURATION_MEDIUM_MS = 1500;
static constexpr uint32_t TOAST_DURATION_LONG_MS = 2500;

// ============================================================================
// Menu Indices
// ============================================================================

enum SettingsMenuIdx {
    SETTINGS_IDX_BRIGHTNESS = 0,
    SETTINGS_IDX_LANGUAGE,
    SETTINGS_IDX_TIMEZONE,
    SETTINGS_IDX_AUTO_SLEEP,
    SETTINGS_IDX_BADGE_TEXT,
    SETTINGS_IDX_SET_DATE,
    SETTINGS_IDX_SET_TIME,
    SETTINGS_IDX_CHANGE_PIN,
    SETTINGS_IDX_COUNT
};

enum LanguageMenuIdx {
    LANG_IDX_ENGLISH = 0,
    LANG_IDX_GERMAN,
    LANG_IDX_COUNT
};

enum WifiMainMenuIdx {
    WIFI_IDX_CONNECT = 0,
    WIFI_IDX_SETUP,
    WIFI_IDX_DETAILS,
    WIFI_IDX_NTP_SYNC,
    WIFI_IDX_COUNT
};

enum WifiAuthIdx {
    WIFI_AUTH_WPA2 = 0,
    WIFI_AUTH_WPA_WPA2,
    WIFI_AUTH_WPA3,
    WIFI_AUTH_WPA,
    WIFI_AUTH_OPEN,
    WIFI_AUTH_WEP,
    WIFI_AUTH_COUNT
};

enum WifiIpModeIdx {
    WIFI_IP_DHCP = 0,
    WIFI_IP_STATIC,
    WIFI_IP_COUNT
};

// ============================================================================
// Static State
// ============================================================================

// UI views (lazy initialized)
static LockScreenView* s_lockScreen = nullptr;
static PinEntryView* s_pinEntry = nullptr;
static ListView* s_mainMenu = nullptr;
static ListView* s_toolsMenu = nullptr;
static ListView* s_settingsMenu = nullptr;
static SliderView* s_brightnessSlider = nullptr;
static SliderView* s_sleepSlider = nullptr;
static SliderView* s_timezoneSlider = nullptr;
static ListView* s_languageMenu = nullptr;
static DateInputView* s_dateInput = nullptr;
static TimeInputView* s_timeInput = nullptr;
static PinChangeView* s_pinChangeView = nullptr;
static ListView* s_expertMenu = nullptr;
static ListView* s_modulesView = nullptr;
static ListView* s_wifiMainMenu = nullptr;
static WifiListView* s_wifiScanView = nullptr;
static ListView* s_wifiAuthMenu = nullptr;
static ListView* s_wifiIpMenu = nullptr;

// Dependencies
static UiDeps s_deps = {};

// Main menu items and module storage
static ListItem s_mainMenuItems[MAIN_MENU_MAX_ITEMS];
static core::ModuleMenuItem s_mainMenuModuleItems[MAIN_MENU_MAX_ITEMS];
static uint8_t s_mainMenuPluginCount = 0;

// Tools menu items
static ListItem s_toolsItems[TOOLS_MAX_ITEMS];
static core::ModuleMenuItem s_toolsModuleItems[TOOLS_MAX_ITEMS];
static uint8_t s_toolsModuleCount = 0;

// Settings menu items
static ListItem s_settingsItems[SETTINGS_IDX_COUNT];

// Language menu items
static ListItem s_languageItems[LANG_IDX_COUNT];

// Expert menu items
static ListItem s_expertItems[EXPERT_COUNT];

// Modules view items
static ListItem s_modulesItems[MODULES_VIEW_MAX];
static char s_moduleLabels[MODULES_VIEW_MAX][48];

// WiFi menu items
static ListItem s_wifiMainItems[WIFI_IDX_COUNT];
static ListItem s_wifiAuthItems[WIFI_AUTH_COUNT];
static ListItem s_wifiIpItems[WIFI_IP_COUNT];
static WifiItem s_wifiScanResults[WIFI_MAX_NETWORKS];
static uint8_t s_wifiScanCount = 0;
static const char* s_wifiAuthLabels[WIFI_AUTH_COUNT] = {
    "WPA2", "WPA/WPA2", "WPA3", "WPA", "Open", "WEP"
};

// RTC update tracking
static int8_t s_lastMinute = -1;

// USB/charging/WiFi/BLE/battery status tracking
static bool s_lastUsbConnected = false;
static bool s_lastCharging = false;
static bool s_lastWifiConnected = false;
static bool s_lastBleEnabled = false;
static bool s_lastBatteryPresent = true;

// Key handling
static bool s_ignoreKeyUntilRelease = false;

// ============================================================================
// Helper Functions (Index Calculation)
// ============================================================================

static inline uint8_t getToolsIndex() { return s_mainMenuPluginCount; }
static inline uint8_t getSettingsIndex() { return s_mainMenuPluginCount + 1; }
static inline uint8_t getMainMenuCount() { return s_mainMenuPluginCount + MAIN_MENU_FIXED_COUNT; }

// ============================================================================
// Forward Declarations
// ============================================================================

static void onUnlockRequested();
static bool onPinVerify(const char* pin);
static void onPinSuccess();
static void onMainMenuSelect(uint16_t index, void* userData);
static void onToolsSelect(uint16_t index, void* userData);
static void onSettingsSelect(uint16_t index, void* userData);
static void onLanguageSelect(uint16_t index, void* userData);
static void rebuildMenuLabels();
static void rebuildMainMenu();
static void rebuildToolsMenu();
static void showModulesView();
static void onInactivityTimeout();
static void clearKeypadBuffer();
static void showWifiMainMenu();
static void onWifiMainSelect(uint16_t index, void* userData);
static void wifiConnect();
static void wifiSetup();
static void wifiShowDetails();
static void wifiDisconnect();
static void wifiNtpSync();
static void wifiStartScan();
static void onWifiScanSelect(uint16_t index, const WifiItem* item);
static void wifiShowAuthMenu();
static void onWifiAuthSelect(uint16_t index, void* userData);
static void wifiShowPasswordInput();
static void onWifiPasswordEntered(const char* password);
static void wifiShowIpModeMenu();
static void onWifiIpModeSelect(uint16_t index, void* userData);
static void wifiShowIpInputField(const char* title, char* target, size_t targetSize,
                                  T9InputView::SaveCallback onComplete);
static void onWifiStaticIpEntered(const char* ip);
static void onWifiGatewayEntered(const char* gateway);
static void onWifiNetmaskEntered(const char* netmask);
static void wifiFinishSetup();
static void rebuildWifiMainMenu();
static void toggleBluetooth();
static void showExpertMenu();
static void runSystemTest();
static void runTropicCacheRebuild();
static void runTropicCacheCleanup();
static void onModuleErrorEvent(const core::Event& evt);

// ============================================================================
// Status Icon Management
// ============================================================================

void updatePowerStatusIcons() {
    if (!s_lockScreen || !s_deps.power) return;

    bool usbConnected = s_deps.power->isUsbConnected();
    hal::ChargeStatus status = s_deps.power->getChargeStatus();
    bool charging = (status == hal::ChargeStatus::FAST_CHARGE ||
                     status == hal::ChargeStatus::PRE_CHARGE);

    if (usbConnected != s_lastUsbConnected) {
        if (usbConnected) s_lockScreen->addStatusIcon(StatusIcon::USB);
        else s_lockScreen->removeStatusIcon(StatusIcon::USB);
        s_lastUsbConnected = usbConnected;
    }

    if (charging != s_lastCharging) {
        if (charging) s_lockScreen->addStatusIcon(StatusIcon::CHARGING);
        else s_lockScreen->removeStatusIcon(StatusIcon::CHARGING);
        s_lastCharging = charging;
    }

    // Battery presence
    bool batteryPresent = s_deps.power->isBatteryPresent();
    if (batteryPresent != s_lastBatteryPresent) {
        if (!batteryPresent) s_lockScreen->addStatusIcon(StatusIcon::NO_BATTERY);
        else s_lockScreen->removeStatusIcon(StatusIcon::NO_BATTERY);
        s_lastBatteryPresent = batteryPresent;
    }

    // WiFi status
    auto* wifi = hal::getWifiControllerInstance();
    bool wifiConnected = wifi && wifi->isConnected();
    if (wifiConnected != s_lastWifiConnected) {
        if (wifiConnected) s_lockScreen->addStatusIcon(StatusIcon::WIFI);
        else s_lockScreen->removeStatusIcon(StatusIcon::WIFI);
        s_lastWifiConnected = wifiConnected;
    }

    // BLE status
    auto* ble = hal::getBluetoothControllerInstance();
    bool bleEnabled = ble && ble->isEnabled();
    if (bleEnabled != s_lastBleEnabled) {
        if (bleEnabled) s_lockScreen->addStatusIcon(StatusIcon::BLE);
        else s_lockScreen->removeStatusIcon(StatusIcon::BLE);
        s_lastBleEnabled = bleEnabled;
    }
}

static void clearKeypadBuffer() {
    if (!s_deps.keypad) return;
    while (s_deps.keypad->getNextKey() != hal::Key::KEY_NONE) {}
}

// ============================================================================
// Clock Update
// ============================================================================

static void updateLockScreenClock() {
    if (!s_lockScreen) return;

    time_t now = time(nullptr);
    struct tm* tm = localtime(&now);
    if (tm && tm->tm_min != s_lastMinute) {
        s_lastMinute = tm->tm_min;
        char buf[40];
        snprintf(buf, sizeof(buf), "%02d:%02d", tm->tm_hour, tm->tm_min);
        s_lockScreen->setClock(buf);
        snprintf(buf, sizeof(buf), "%02d.%02d.%04d", tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900);
        s_lockScreen->setDate(buf);
        IView* currentView = ViewStack::instance().current();
        if (currentView) currentView->markDirty();
    }
}

// ============================================================================
// Lock/Unlock Handlers
// ============================================================================

static void onUnlockRequested() {
    s_ignoreKeyUntilRelease = true;
    clearKeypadBuffer();
    if (s_deps.display) {
        s_deps.display->backlightOn();
    }
    if (s_pinEntry) {
        s_pinEntry->clear();
        ViewStack::instance().push(s_pinEntry);
    }
}

static bool onPinVerify(const char* pin) {
    return core::PinManager::instance().verifyBadgePin(pin);
}

static void onPinSuccess() {
    if (s_deps.display) {
        s_deps.display->backlightOn();
    }
    if (s_mainMenu) {
        ViewStack::instance().replace(s_mainMenu);
    } else {
        ViewStack::instance().pop();
    }
}

static void onInactivityTimeout() {
    if (s_deps.display) {
        s_deps.display->backlightOff();
    }
    ViewStack::instance().popToRoot();
    ViewStack::instance().resetInactivityTimer();
    SleepManager::instance().resetTimer();
    updatePowerStatusIcons();
}

// ============================================================================
// Menu Building
// ============================================================================

static void rebuildMainMenu() {
    auto& moduleReg = core::ModuleRegistry::instance();

    // Get module items for main menu
    s_mainMenuPluginCount = moduleReg.getMenuItems(
        core::MenuLocation::MAIN_MENU,
        s_mainMenuModuleItems,
        MAIN_MENU_MAX_ITEMS - MAIN_MENU_FIXED_COUNT
    );

    // Build menu: Module items first, then Tools, Settings
    for (uint8_t i = 0; i < s_mainMenuPluginCount; i++) {
        s_mainMenuItems[i] = {s_mainMenuModuleItems[i].label, 0, false, nullptr};
    }
    s_mainMenuItems[getToolsIndex()] = {tr(StringId::TOOLS), 0, false, nullptr};
    s_mainMenuItems[getSettingsIndex()] = {tr(StringId::SETTINGS), 0, false, nullptr};

    if (s_mainMenu) {
        s_mainMenu->init(tr(StringId::MAIN_MENU), s_mainMenuItems, getMainMenuCount());
    }
}

static void rebuildToolsMenu() {
    auto& moduleReg = core::ModuleRegistry::instance();

    // Fixed items: Modules, WiFi, Bluetooth, Expert
    s_toolsItems[0] = {tr(StringId::MODULES), 0, false, nullptr};
    s_toolsItems[1] = {tr(StringId::WIFI_MENU), 0, false, nullptr};

    // Bluetooth status indicator
    auto* ble = hal::getBluetoothControllerInstance();
    if (ble && ble->isEnabled()) {
        s_toolsItems[2] = {tr(StringId::BLUETOOTH_ON), '*', false, nullptr};
    } else {
        s_toolsItems[2] = {tr(StringId::BLUETOOTH_OFF), 0, false, nullptr};
    }

    s_toolsItems[3] = {tr(StringId::EXPERT), 0, false, nullptr};

    // Get module items for tools menu
    s_toolsModuleCount = moduleReg.getMenuItems(
        core::MenuLocation::TOOLS_MENU,
        s_toolsModuleItems,
        TOOLS_MAX_ITEMS - TOOLS_FIXED_COUNT
    );

    // Add module items after fixed items
    for (uint8_t i = 0; i < s_toolsModuleCount; i++) {
        s_toolsItems[TOOLS_FIXED_COUNT + i] = {s_toolsModuleItems[i].label, 0, false, nullptr};
    }

    if (s_toolsMenu) {
        s_toolsMenu->init(tr(StringId::TOOLS), s_toolsItems, TOOLS_FIXED_COUNT + s_toolsModuleCount);
    }
}

static void rebuildMenuLabels() {
    rebuildMainMenu();
    rebuildToolsMenu();

    s_settingsItems[SETTINGS_IDX_BRIGHTNESS].label = tr(StringId::BRIGHTNESS);
    s_settingsItems[SETTINGS_IDX_LANGUAGE].label = tr(StringId::LANGUAGE);
    s_settingsItems[SETTINGS_IDX_TIMEZONE].label = tr(StringId::TIMEZONE);
    s_settingsItems[SETTINGS_IDX_AUTO_SLEEP].label = tr(StringId::AUTO_SLEEP);
    s_settingsItems[SETTINGS_IDX_BADGE_TEXT].label = tr(StringId::BADGE_TEXT);
    s_settingsItems[SETTINGS_IDX_SET_DATE].label = tr(StringId::SET_DATE);
    s_settingsItems[SETTINGS_IDX_SET_TIME].label = tr(StringId::SET_TIME);
    s_settingsItems[SETTINGS_IDX_CHANGE_PIN].label = tr(StringId::CHANGE_PIN);
    if (s_settingsMenu) {
        s_settingsMenu->init(tr(StringId::SETTINGS), s_settingsItems, SETTINGS_IDX_COUNT);
    }

    if (s_brightnessSlider) {
        uint16_t currentBrightness = s_deps.display ? s_deps.display->getBacklight() / 10 : 50;
        s_brightnessSlider->init(tr(StringId::BRIGHTNESS), 0, 100, currentBrightness, 1, "%");
        s_brightnessSlider->setStepCallback(settings::brightnessStepCallback);
        s_brightnessSlider->setOnSave(settings::onBrightnessSave);
        s_brightnessSlider->setOnChange(settings::onBrightnessChange);
    }

    if (s_sleepSlider) {
        uint16_t currentSleepMin = 0;
        if (s_deps.sleep) {
            currentSleepMin = static_cast<uint16_t>(s_deps.sleep->getLightSleepInterval() / 60);
        }
        s_sleepSlider->init(tr(StringId::AUTO_SLEEP), 0, 60, currentSleepMin, 1, tr(StringId::MINUTES));
        s_sleepSlider->setZeroLabel(tr(StringId::NEVER));
        s_sleepSlider->setOnSave(settings::onSleepIntervalSave);
    }

    if (s_languageMenu) {
        s_languageItems[LANG_IDX_ENGLISH].label = I18n::instance().getLanguageName(Language::EN);
        s_languageItems[LANG_IDX_GERMAN].label = I18n::instance().getLanguageName(Language::DE);
        s_languageMenu->init(tr(StringId::LANGUAGE), s_languageItems, LANG_IDX_COUNT);
    }
}

// ============================================================================
// Menu Selection Handlers
// ============================================================================

static void onMainMenuSelect(uint16_t index, void* userData) {
    (void)userData;

    // Module items first
    if (index < s_mainMenuPluginCount) {
        auto& item = s_mainMenuModuleItems[index];
        if (item.getView) {
            IView* view = item.getView();
            if (view) {
                ViewStack::instance().push(view);
            }
        }
        return;
    }

    // Fixed items: Tools, Settings
    if (index == getToolsIndex()) {
        if (s_toolsMenu) ViewStack::instance().push(s_toolsMenu);
    } else if (index == getSettingsIndex()) {
        if (s_settingsMenu) ViewStack::instance().push(s_settingsMenu);
    }
}

static void onToolsSelect(uint16_t index, void* userData) {
    (void)userData;

    // Fixed items
    switch (index) {
        case 0:  // Modules
            showModulesView();
            return;
        case 1:  // WiFi
            showWifiMainMenu();
            return;
        case 2:  // Bluetooth
            toggleBluetooth();
            return;
        case 3:  // Expert
            showExpertMenu();
            return;
    }

    // Module items
    uint8_t moduleIdx = index - TOOLS_FIXED_COUNT;
    if (moduleIdx < s_toolsModuleCount) {
        auto& item = s_toolsModuleItems[moduleIdx];
        if (item.getView) {
            IView* view = item.getView();
            if (view) {
                ViewStack::instance().push(view);
            }
        }
    }
}

static void onSettingsSelect(uint16_t index, void* userData) {
    (void)userData;
    switch (index) {
        case SETTINGS_IDX_BRIGHTNESS:
            if (s_brightnessSlider) {
                uint16_t currentBrightness = s_deps.display ? s_deps.display->getBacklight() / 10 : 50;
                s_brightnessSlider->setValue(currentBrightness);
                ViewStack::instance().push(s_brightnessSlider);
            }
            break;
        case SETTINGS_IDX_LANGUAGE:
            if (s_languageMenu) {
                Language currentLang = I18n::instance().getLanguage();
                s_languageItems[LANG_IDX_ENGLISH].icon = (currentLang == Language::EN) ? '*' : 0;
                s_languageItems[LANG_IDX_GERMAN].icon = (currentLang == Language::DE) ? '*' : 0;
                ViewStack::instance().push(s_languageMenu);
            }
            break;
        case SETTINGS_IDX_TIMEZONE:
            if (s_timezoneSlider) {
                auto* rtc = hal::getRtcInstance();
                int8_t currentTz = rtc ? rtc->getTimezoneOffset() : 0;
                uint16_t sliderVal = static_cast<uint16_t>(currentTz + 12);
                s_timezoneSlider->setValue(sliderVal);
                ViewStack::instance().push(s_timezoneSlider);
            }
            break;
        case SETTINGS_IDX_AUTO_SLEEP:
            if (s_sleepSlider) {
                uint16_t currentSleepMin = 0;
                if (s_deps.sleep) {
                    currentSleepMin = static_cast<uint16_t>(s_deps.sleep->getLightSleepInterval() / 60);
                }
                s_sleepSlider->setValue(currentSleepMin);
                ViewStack::instance().push(s_sleepSlider);
            }
            break;
        case SETTINGS_IDX_BADGE_TEXT:
            settings::startBadgeTextEdit();
            break;
        case SETTINGS_IDX_SET_DATE: {
            if (s_dateInput) {
                time_t now = time(nullptr);
                struct tm* tm = localtime(&now);
                if (tm) {
                    s_dateInput->init(tr(StringId::SET_DATE), tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900);
                }
                ViewStack::instance().push(s_dateInput);
            }
            break;
        }
        case SETTINGS_IDX_SET_TIME: {
            if (s_timeInput) {
                time_t now = time(nullptr);
                struct tm* tm = localtime(&now);
                if (tm) {
                    s_timeInput->init(tr(StringId::SET_TIME), tm->tm_hour, tm->tm_min);
                }
                ViewStack::instance().push(s_timeInput);
            }
            break;
        }
        case SETTINGS_IDX_CHANGE_PIN:
            if (s_pinChangeView) {
                s_pinChangeView->init(core::PinManager::BADGE_PIN_MIN, core::PinManager::BADGE_PIN_MAX);
                ViewStack::instance().push(s_pinChangeView);
            }
            break;
    }
}

static void onLanguageSelect(uint16_t index, void* userData) {
    (void)userData;
    Language newLang = Language::EN;
    switch (index) {
        case LANG_IDX_ENGLISH: newLang = Language::EN; break;
        case LANG_IDX_GERMAN: newLang = Language::DE; break;
    }
    I18n::instance().setLanguage(newLang);
    rebuildMenuLabels();
    ViewStack::instance().pop();
}

// ============================================================================
// Modules View
// ============================================================================

static void rebuildModulesView();

static void onModuleRetryConfirm(void* userData) {
    uint8_t index = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(userData));
    auto& moduleReg = core::ModuleRegistry::instance();

    if (moduleReg.retryModule(index)) {
        showToastSuccess("OK", TOAST_DURATION_SHORT_MS);
    } else {
        const char* error = moduleReg.getModuleSlotError(index);
        showToastError(error ? error : tr(StringId::FAILED), TOAST_DURATION_MEDIUM_MS);
    }

    ui_rebuild_menus();
    rebuildModulesView();
}

static void onModuleSelect(uint16_t index, void* userData) {
    (void)userData;

    auto& moduleReg = core::ModuleRegistry::instance();
    if (index >= moduleReg.getModuleCount()) return;

    core::IModule* module = moduleReg.getModuleAt(index);
    if (!module) return;

    uint8_t idx = static_cast<uint8_t>(index);

    // If module has error, show retry dialog
    if (moduleReg.hasModuleSlotError(idx)) {
        const char* error = moduleReg.getModuleSlotError(idx);

        static char confirmMsg[128];
        snprintf(confirmMsg, sizeof(confirmMsg), "%s\n\nNochmal laden?",
                 error ? error : "Modul-Fehler");

        showConfirm(confirmMsg, onModuleRetryConfirm, nullptr,
                    ConfirmView::Icon::ERROR, reinterpret_cast<void*>(static_cast<uintptr_t>(idx)));
        return;
    }

    // Remember USB state before toggle
    bool needsReplugBefore = core::UsbManager::instance().needsReplug();

    // Normal toggle: enable/disable module
    bool nowEnabled = moduleReg.toggleModuleEnabled(idx);

    if (nowEnabled) {
        if (!moduleReg.startModule(idx)) {
            const char* error = moduleReg.getModuleSlotError(idx);
            if (error) {
                showToastError(error, TOAST_DURATION_MEDIUM_MS);
            } else {
                showToastError(tr(StringId::FAILED), TOAST_DURATION_MEDIUM_MS);
            }
        }
    } else {
        if (module->getState() == core::ServiceState::STARTED) {
            module->stop();
        }
    }

    // If USB config changed by THIS module toggle, show sticky alert
    bool needsReplugAfter = core::UsbManager::instance().needsReplug();
    if (!needsReplugBefore && needsReplugAfter) {
        showToastAlertSticky(tr(StringId::USB_REPLUG_REQUIRED));
    }

    ui_rebuild_menus();
    rebuildModulesView();
}

static void rebuildModulesView() {
    auto& moduleReg = core::ModuleRegistry::instance();
    uint8_t count = moduleReg.getModuleCount();

    if (count == 0) {
        s_modulesItems[0] = {"(none)", 0, false, nullptr};
        count = 1;
    } else {
        for (uint8_t i = 0; i < count && i < MODULES_VIEW_MAX; i++) {
            core::IModule* module = moduleReg.getModuleAt(i);
            if (module) {
                const char* status;
                if (moduleReg.hasModuleSlotError(i)) {
                    status = "[FAIL]";
                } else {
                    bool enabled = moduleReg.isModuleEnabled(i);
                    status = enabled
                        ? (module->getState() == core::ServiceState::STARTED ? "[ON]" : "[--]")
                        : "[OFF]";
                }
                snprintf(s_moduleLabels[i], sizeof(s_moduleLabels[i]),
                         "%s %s", module->getName(), status);
                s_modulesItems[i] = {s_moduleLabels[i], 0, false, nullptr};
            }
        }
        if (count > MODULES_VIEW_MAX) count = MODULES_VIEW_MAX;
    }

    if (s_modulesView) {
        s_modulesView->init(tr(StringId::MODULES), s_modulesItems, count);
    }
}

static void showModulesView() {
    if (!s_modulesView) {
        s_modulesView = new ListView();
    }

    rebuildModulesView();
    s_modulesView->setOnSelect(onModuleSelect);
    ViewStack::instance().push(s_modulesView);
}

// ============================================================================
// WiFi Management
// ============================================================================

static void rebuildWifiMainMenu() {
    auto* wifi = hal::getWifiControllerInstance();
    bool connected = wifi && wifi->isConnected();
    auto& wifiHandlers = WifiHandlers::instance();
    bool hasConfig = wifiHandlers.config().valid;

    if (connected) {
        s_wifiMainItems[WIFI_IDX_CONNECT] = {tr(StringId::WIFI_DISCONNECT), '*', false, nullptr};
    } else if (hasConfig) {
        s_wifiMainItems[WIFI_IDX_CONNECT] = {tr(StringId::WIFI_CONNECT), 0, false, nullptr};
    } else {
        s_wifiMainItems[WIFI_IDX_CONNECT] = {tr(StringId::WIFI_NO_CONFIG), 0, true, nullptr};
    }

    s_wifiMainItems[WIFI_IDX_SETUP] = {tr(StringId::WIFI_SETUP), 0, false, nullptr};
    s_wifiMainItems[WIFI_IDX_DETAILS] = {tr(StringId::WIFI_DETAILS), 0, false, nullptr};
    s_wifiMainItems[WIFI_IDX_NTP_SYNC] = {tr(StringId::NTP_SYNC), 0, !connected && !hasConfig, nullptr};

    if (s_wifiMainMenu) {
        s_wifiMainMenu->init(tr(StringId::WIFI_MENU), s_wifiMainItems, WIFI_IDX_COUNT);
    }
}

static void showWifiMainMenu() {
    WifiHandlers::instance().loadConfig();

    if (!s_wifiMainMenu) {
        s_wifiMainMenu = new ListView();
        s_wifiMainMenu->setOnSelect(onWifiMainSelect);
    }

    rebuildWifiMainMenu();
    ViewStack::instance().push(s_wifiMainMenu);
}

static void onWifiMainSelect(uint16_t index, void* userData) {
    (void)userData;

    switch (index) {
        case WIFI_IDX_CONNECT: {
            auto* wifi = hal::getWifiControllerInstance();
            if (wifi && wifi->isConnected()) {
                wifiDisconnect();
            } else {
                wifiConnect();
            }
            break;
        }
        case WIFI_IDX_SETUP:
            wifiSetup();
            break;
        case WIFI_IDX_DETAILS:
            wifiShowDetails();
            break;
        case WIFI_IDX_NTP_SYNC:
            wifiNtpSync();
            break;
    }
}

static void wifiConnect() {
    auto& wifiHandlers = WifiHandlers::instance();

    if (!wifiHandlers.config().valid) {
        showToastError(tr(StringId::WIFI_NO_CONFIG));
        return;
    }

    char msg[96];
    snprintf(msg, sizeof(msg), "%s: %s", tr(StringId::WIFI_CONNECTING), wifiHandlers.config().ssid);
    showToastInfo(msg, 0);

    bool connected = wifiHandlers.connect();
    ViewStack::instance().hideModal();

    if (connected) {
        auto* wifi = hal::getWifiControllerInstance();
        char ipBuf[20] = {};
        if (wifi) wifi->getIpAddress(ipBuf, sizeof(ipBuf));
        snprintf(msg, sizeof(msg), "%s (IP: %s)", tr(StringId::WIFI_CONNECTED), ipBuf);
        showToastSuccess(msg, TOAST_DURATION_LONG_MS);
    } else {
        snprintf(msg, sizeof(msg), "%s: %s", tr(StringId::WIFI_FAILED), wifiHandlers.getLastError());
        showToastError(msg, TOAST_DURATION_LONG_MS);
    }

    rebuildWifiMainMenu();
}

static void wifiSetup() {
    WifiHandlers::instance().wizard().reset();
    wifiStartScan();
}

static void wifiStartScan() {
    auto* wifi = hal::getWifiControllerInstance();
    if (!wifi) {
        showToastError(tr(StringId::HW_NOT_AVAILABLE));
        return;
    }

    if (!wifi->isEnabled()) {
        wifi->enable(hal::WifiMode::STA);
    }

    showToastInfo(tr(StringId::WIFI_SCANNING), 0);

    s_wifiScanCount = 0;
    if (wifi->startScan()) {
        uint32_t startMs = esp_timer_get_time() / 1000;
        while (!wifi->isScanComplete()) {
            vTaskDelay(pdMS_TO_TICKS(100));
            if ((esp_timer_get_time() / 1000 - startMs) > WIFI_SCAN_TIMEOUT_MS) break;
        }

        hal::WifiScanResult rawResults[WIFI_MAX_NETWORKS];
        uint8_t rawCount = wifi->getScanResults(rawResults, WIFI_MAX_NETWORKS);

        // Convert and deduplicate
        for (uint8_t i = 0; i < rawCount && s_wifiScanCount < WIFI_MAX_NETWORKS; i++) {
            if (rawResults[i].ssid[0] == '\0') continue;

            bool found = false;
            for (uint8_t j = 0; j < s_wifiScanCount; j++) {
                if (strcmp(s_wifiScanResults[j].ssid, rawResults[i].ssid) == 0) {
                    if (rawResults[i].rssi > s_wifiScanResults[j].rssi) {
                        s_wifiScanResults[j].rssi = rawResults[i].rssi;
                        s_wifiScanResults[j].security = rawResults[i].security;
                    }
                    found = true;
                    break;
                }
            }

            if (!found) {
                strncpy(s_wifiScanResults[s_wifiScanCount].ssid, rawResults[i].ssid, 32);
                s_wifiScanResults[s_wifiScanCount].ssid[32] = '\0';
                s_wifiScanResults[s_wifiScanCount].rssi = rawResults[i].rssi;
                s_wifiScanResults[s_wifiScanCount].security = rawResults[i].security;
                s_wifiScanCount++;
            }
        }
    }

    ViewStack::instance().hideModal();

    if (!s_wifiScanView) {
        s_wifiScanView = new WifiListView();
        s_wifiScanView->setOnSelect(onWifiScanSelect);
    }

    s_wifiScanView->init(tr(StringId::WIFI_SETUP), s_wifiScanResults, s_wifiScanCount);
    ViewStack::instance().push(s_wifiScanView);
}

static void onWifiScanSelect(uint16_t index, const WifiItem* item) {
    auto& wizard = WifiHandlers::instance().wizard();

    if (index == 0 || item == nullptr) {
        wizard.fromScan = false;
        showT9Input(tr(StringId::WIFI_SSID), "", [](const char* ssid) {
            strncpy(WifiHandlers::instance().wizard().ssid, ssid,
                    sizeof(WifiHandlers::instance().wizard().ssid) - 1);
            wifiShowAuthMenu();
        }, 32);
        return;
    }

    wizard.fromScan = true;
    strncpy(wizard.ssid, item->ssid, sizeof(wizard.ssid) - 1);
    wizard.security = item->security;

    if (wizard.security == hal::WifiSecurity::OPEN) {
        wizard.password[0] = '\0';
        wifiShowIpModeMenu();
    } else {
        wifiShowPasswordInput();
    }
}

static void wifiShowAuthMenu() {
    if (!s_wifiAuthMenu) {
        s_wifiAuthMenu = new ListView();
        s_wifiAuthMenu->setOnSelect(onWifiAuthSelect);

        for (uint8_t i = 0; i < WIFI_AUTH_COUNT; i++) {
            s_wifiAuthItems[i] = {s_wifiAuthLabels[i], 0, false, nullptr};
        }
    }

    s_wifiAuthMenu->init(tr(StringId::WIFI_ENCRYPTION), s_wifiAuthItems, WIFI_AUTH_COUNT);
    ViewStack::instance().push(s_wifiAuthMenu);
}

static void onWifiAuthSelect(uint16_t index, void* userData) {
    (void)userData;
    auto& wizard = WifiHandlers::instance().wizard();

    switch (index) {
        case WIFI_AUTH_WPA2:     wizard.security = hal::WifiSecurity::WPA2_PSK; break;
        case WIFI_AUTH_WPA_WPA2: wizard.security = hal::WifiSecurity::WPA2_PSK; break;
        case WIFI_AUTH_WPA3:     wizard.security = hal::WifiSecurity::WPA3_PSK; break;
        case WIFI_AUTH_WPA:      wizard.security = hal::WifiSecurity::WPA_PSK; break;
        case WIFI_AUTH_OPEN:     wizard.security = hal::WifiSecurity::OPEN; break;
        case WIFI_AUTH_WEP:      wizard.security = hal::WifiSecurity::WEP; break;
    }

    if (wizard.security == hal::WifiSecurity::OPEN) {
        wizard.password[0] = '\0';
        wifiShowIpModeMenu();
    } else {
        wifiShowPasswordInput();
    }
}

static void wifiShowPasswordInput() {
    showT9Input(tr(StringId::WIFI_PASSWORD), "", onWifiPasswordEntered, 64);
}

static void onWifiPasswordEntered(const char* password) {
    auto& wizard = WifiHandlers::instance().wizard();
    strncpy(wizard.password, password, sizeof(wizard.password) - 1);
    wifiShowIpModeMenu();
}

static void wifiShowIpModeMenu() {
    if (!s_wifiIpMenu) {
        s_wifiIpMenu = new ListView();
        s_wifiIpMenu->setOnSelect(onWifiIpModeSelect);
    }

    s_wifiIpItems[WIFI_IP_DHCP] = {tr(StringId::WIFI_DHCP), 0, false, nullptr};
    s_wifiIpItems[WIFI_IP_STATIC] = {tr(StringId::WIFI_STATIC), 0, false, nullptr};

    s_wifiIpMenu->init(tr(StringId::WIFI_IP_MODE), s_wifiIpItems, WIFI_IP_COUNT);
    ViewStack::instance().push(s_wifiIpMenu);
}

static void onWifiIpModeSelect(uint16_t index, void* userData) {
    (void)userData;
    auto& wizard = WifiHandlers::instance().wizard();

    wizard.useDhcp = (index == WIFI_IP_DHCP);

    if (wizard.useDhcp) {
        wifiFinishSetup();
    } else {
        wifiShowIpInputField("IP", wizard.staticIp, sizeof(wizard.staticIp), onWifiStaticIpEntered);
    }
}

static void wifiShowIpInputField(const char* title, char* target, size_t targetSize,
                                  T9InputView::SaveCallback onComplete) {
    (void)targetSize;
    showT9Input(title, target, onComplete, 15);
}

static void onWifiStaticIpEntered(const char* ip) {
    auto& wizard = WifiHandlers::instance().wizard();
    if (!WifiHandlers::isValidIpAddress(ip)) {
        showToastError("Invalid IP", TOAST_DURATION_MEDIUM_MS);
        wifiShowIpInputField("IP", wizard.staticIp, sizeof(wizard.staticIp), onWifiStaticIpEntered);
        return;
    }
    strncpy(wizard.staticIp, ip, sizeof(wizard.staticIp) - 1);
    wifiShowIpInputField(tr(StringId::WIFI_GATEWAY), wizard.gateway, sizeof(wizard.gateway), onWifiGatewayEntered);
}

static void onWifiGatewayEntered(const char* gateway) {
    auto& wizard = WifiHandlers::instance().wizard();
    if (!WifiHandlers::isValidIpAddress(gateway)) {
        showToastError("Invalid IP", TOAST_DURATION_MEDIUM_MS);
        wifiShowIpInputField(tr(StringId::WIFI_GATEWAY), wizard.gateway, sizeof(wizard.gateway), onWifiGatewayEntered);
        return;
    }
    strncpy(wizard.gateway, gateway, sizeof(wizard.gateway) - 1);
    wifiShowIpInputField(tr(StringId::WIFI_NETMASK), wizard.netmask, sizeof(wizard.netmask), onWifiNetmaskEntered);
}

static void onWifiNetmaskEntered(const char* netmask) {
    auto& wizard = WifiHandlers::instance().wizard();
    if (!WifiHandlers::isValidIpAddress(netmask)) {
        showToastError("Invalid IP", TOAST_DURATION_MEDIUM_MS);
        wifiShowIpInputField(tr(StringId::WIFI_NETMASK), wizard.netmask, sizeof(wizard.netmask), onWifiNetmaskEntered);
        return;
    }
    strncpy(wizard.netmask, netmask, sizeof(wizard.netmask) - 1);
    wifiFinishSetup();
}

static void wifiFinishSetup() {
    WifiHandlers::instance().saveConfig();

    while (ViewStack::instance().current() != s_wifiMainMenu &&
           ViewStack::instance().depth() > 1) {
        ViewStack::instance().pop();
    }

    wifiConnect();
}

// Static buffer for WiFi details (PSRAM)
static constexpr size_t WIFI_DETAILS_BUF_SIZE = 512;
static EXT_RAM_BSS_ATTR char s_wifiDetailsBuf[WIFI_DETAILS_BUF_SIZE];

static void wifiShowDetails() {
    auto* wifi = hal::getWifiControllerInstance();
    auto& wifiHandlers = WifiHandlers::instance();
    char* info = s_wifiDetailsBuf;
    memset(info, 0, WIFI_DETAILS_BUF_SIZE);
    size_t pos = 0;

    auto append = [&](const char* fmt, ...) {
        if (pos >= WIFI_DETAILS_BUF_SIZE) return;
        va_list args;
        va_start(args, fmt);
        int written = vsnprintf(info + pos, WIFI_DETAILS_BUF_SIZE - pos, fmt, args);
        va_end(args);
        if (written > 0) {
            size_t w = static_cast<size_t>(written);
            pos += (w < (WIFI_DETAILS_BUF_SIZE - pos)) ? w : (WIFI_DETAILS_BUF_SIZE - pos - 1);
        }
    };

    if (wifi && wifi->isConnected()) {
        char ipBuf[20] = {};
        uint8_t mac[6] = {};

        append("Status: %s\n\n", tr(StringId::WIFI_CONNECTED));
        append("SSID: %s\n", wifi->getCurrentSsid());

        if (wifi->getMacAddress(mac)) {
            append("MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                   mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        }

        if (wifi->getIpAddress(ipBuf, sizeof(ipBuf))) {
            append("IP: %s\n", ipBuf);
        }

        append("%s: %d dBm\n", tr(StringId::WIFI_SIGNAL), wifi->getRssi());

    } else if (wifiHandlers.config().valid) {
        append("Status: %s\n\n", tr(StringId::WIFI_DISCONNECTED));
        append("=== %s ===\n", tr(StringId::WIFI_SAVED_CONFIG));
        append("SSID: %s\n", wifiHandlers.config().ssid);
        append("IP: %s\n", wifiHandlers.config().useDhcp ? "DHCP" : "Static");
    } else {
        append("%s", tr(StringId::WIFI_NO_CONFIG));
    }

    showInfo(tr(StringId::WIFI_DETAILS), info);
}

static void wifiDisconnect() {
    WifiHandlers::instance().disconnect();
    showToastInfo(tr(StringId::WIFI_DISCONNECTED));
    rebuildWifiMainMenu();
}

static void wifiNtpSync() {
    auto& wifiHandlers = WifiHandlers::instance();

    if (!wifiHandlers.config().valid && !wifiHandlers.isConnected()) {
        showToastError(tr(StringId::WIFI_NO_CONFIG));
        return;
    }

    bool wasConnected = wifiHandlers.isConnected();

    if (!wasConnected) {
        showToastTask(tr(StringId::WIFI_CONNECTING), 0);
        if (!wifiHandlers.connect()) {
            ViewStack::instance().hideModal();
            showToastError(tr(StringId::WIFI_FAILED));
            return;
        }
        ViewStack::instance().hideModal();
    }

    showToastTask(tr(StringId::NTP_SYNCING), 0);

    bool synced = wifiHandlers.syncNtp();
    ViewStack::instance().hideModal();

    if (synced) {
        // Update lock screen clock
        time_t now = time(nullptr);
        struct tm* tm = localtime(&now);
        if (tm && s_lockScreen) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%02d:%02d", tm->tm_hour, tm->tm_min);
            s_lockScreen->setClock(buf);
            snprintf(buf, sizeof(buf), "%02d.%02d.%04d", tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900);
            s_lockScreen->setDate(buf);
        }
        showToastSuccess(tr(StringId::NTP_SUCCESS));
    } else {
        showToastError(tr(StringId::NTP_TIMEOUT));
    }
}

// ============================================================================
// Bluetooth Toggle
// ============================================================================

static void toggleBluetooth() {
    auto* ble = hal::getBluetoothControllerInstance();
    if (!ble) {
        showToastError(tr(StringId::HW_NOT_AVAILABLE));
        return;
    }

    if (ble->isEnabled()) {
        ble->disable();
        showToastInfo(tr(StringId::BLUETOOTH_OFF));
    } else {
        if (ble->enable()) {
            showToastSuccess(tr(StringId::BLUETOOTH_ON));
        } else {
            showToastError(tr(StringId::FAILED));
        }
    }

    rebuildToolsMenu();
}

// ============================================================================
// Expert Menu
// ============================================================================

static void runSystemTest() {
    showHardwareInfo();
}

static void runTropicCacheRebuild() {
    showToastTask(tr(StringId::TASK_WORKING), 0);
    bool ok = core::TropicStorage::instance().rebuild();
    ViewStack::instance().hideModal();
    if (ok) {
        showToastSuccess(tr(StringId::OK));
    } else {
        showToastError(tr(StringId::FAILED));
    }
}

static void runTropicCacheCleanup() {
    showToastTask(tr(StringId::TASK_WORKING), 0);
    bool ok = core::TropicStorage::instance().cleanup();
    ViewStack::instance().hideModal();
    if (ok) {
        showToastSuccess(tr(StringId::OK));
    } else {
        showToastError(tr(StringId::FAILED));
    }
}

static void showExpertMenu() {
    showToastInfo(tr(StringId::EXPERT_WARNING), TOAST_DURATION_MEDIUM_MS);
    if (!s_expertMenu) {
        s_expertMenu = new ListView();
        s_expertMenu->setOnSelect([](uint16_t index, void* userData) {
            (void)userData;
            switch (index) {
                case 0: runSystemTest(); break;
                case 1: runTropicCacheRebuild(); break;
                case 2: runTropicCacheCleanup(); break;
            }
        });
    }

    s_expertItems[0] = {tr(StringId::HARDWARE_INFO), 0, false, nullptr};
    s_expertItems[1] = {tr(StringId::TR01_CACHE_REBUILD), 0, false, nullptr};
    s_expertItems[2] = {tr(StringId::TR01_CACHE_CLEANUP), 0, false, nullptr};

    s_expertMenu->init(tr(StringId::EXPERT), s_expertItems, EXPERT_COUNT);
    ViewStack::instance().push(s_expertMenu);
}

// ============================================================================
// Module Error Event Handler
// ============================================================================

static void onModuleErrorEvent(const core::Event& evt) {
    if (evt.type != core::EventType::MODULE_ERROR) return;

    auto& moduleReg = core::ModuleRegistry::instance();
    uint8_t index = static_cast<uint8_t>(evt.data.value);

    if (index >= moduleReg.getModuleCount()) return;

    const char* error = moduleReg.getModuleSlotError(index);
    core::IModule* module = moduleReg.getModuleAt(index);
    const char* name = module ? module->getName() : "?";

    static char errMsg[96];
    snprintf(errMsg, sizeof(errMsg), "%s: %s", name, error ? error : "Fehler");

    showToastError(errMsg, TOAST_DURATION_LONG_MS);
}

// ============================================================================
// Public API Implementation
// ============================================================================

void ui_init(const UiDeps& deps) {
    s_deps = deps;

    // Initialize I18n
    I18n::instance().init();

    // Create LockScreen
    s_lockScreen = new LockScreenView();
    s_lockScreen->init();

    if (s_deps.keypad) {
        s_deps.keypad->setLongPressEnabled(true, 800);
        s_deps.keypad->setLongPressCallback([](hal::Key key) {
            char keyChar = static_cast<char>(key);
            ViewStack::instance().dispatchLongPress(keyChar);
        });
    }

    // Load display texts from NVS
    {
        nvs_handle_t nvs;
        char buf[64];
        size_t len;

        if (nvs_open("display", NVS_READONLY, &nvs) == ESP_OK) {
            len = sizeof(buf);
            if (nvs_get_str(nvs, "name", buf, &len) == ESP_OK && len > 1) {
                s_lockScreen->setDisplayName(buf);
            } else {
                s_lockScreen->setDisplayName(tr(StringId::DEFAULT_NAME));
            }

            len = sizeof(buf);
            if (nvs_get_str(nvs, "info", buf, &len) == ESP_OK && len > 1) {
                s_lockScreen->setInfo(buf);
            } else {
                s_lockScreen->setInfo(tr(StringId::DEFAULT_INFO));
            }

            len = sizeof(buf);
            if (nvs_get_str(nvs, "info2", buf, &len) == ESP_OK && len > 1) {
                s_lockScreen->setInfo2(buf);
            }

            nvs_close(nvs);
        } else {
            s_lockScreen->setDisplayName(tr(StringId::DEFAULT_NAME));
            s_lockScreen->setInfo(tr(StringId::DEFAULT_INFO));
        }
    }

    s_lockScreen->setOnUnlock(onUnlockRequested);

    if (!core::TropicSlotMap::instance().isValid()) {
        const char* msg = core::TropicSlotMap::instance().errorMessage();
        showToastAlertSticky(msg ? msg : "Slot map invalid");
    }

    // Set initial clock/date from RTC
    {
        time_t now = time(nullptr);
        struct tm* t = localtime(&now);
        if (t) {
            char buf[40];
            snprintf(buf, sizeof(buf), "%02d:%02d", t->tm_hour, t->tm_min);
            s_lockScreen->setClock(buf);
            snprintf(buf, sizeof(buf), "%02d.%02d.%04d", t->tm_mday, t->tm_mon + 1, t->tm_year + 1900);
            s_lockScreen->setDate(buf);
            s_lastMinute = t->tm_min;
        }
    }

    // Set initial battery/charging status
    if (s_deps.power) {
        s_lockScreen->setBatteryPercent(s_deps.power->getBatteryPercent());
        if (s_deps.power->getChargeStatus() == hal::ChargeStatus::FAST_CHARGE ||
            s_deps.power->getChargeStatus() == hal::ChargeStatus::PRE_CHARGE) {
            s_lockScreen->addStatusIcon(StatusIcon::CHARGING);
        }
        if (s_deps.power->isUsbConnected()) {
            s_lockScreen->addStatusIcon(StatusIcon::USB);
        }
        s_lastUsbConnected = s_deps.power->isUsbConnected();
        s_lastCharging = (s_deps.power->getChargeStatus() == hal::ChargeStatus::FAST_CHARGE ||
                          s_deps.power->getChargeStatus() == hal::ChargeStatus::PRE_CHARGE);
    }

    // Create PinEntryView
    s_pinEntry = new PinEntryView();
    s_pinEntry->init(tr(StringId::ENTER_PIN), 8, 3);
    s_pinEntry->setOnVerify(onPinVerify);
    s_pinEntry->setOnSuccess(onPinSuccess);

    // Create Main Menu
    s_mainMenu = new ListView();
    s_mainMenu->setOnSelect(onMainMenuSelect);

    // Tools Menu
    s_toolsMenu = new ListView();
    s_toolsMenu->setOnSelect(onToolsSelect);

    // Build menus with module items
    rebuildMainMenu();
    rebuildToolsMenu();

    // Settings Menu
    s_settingsItems[SETTINGS_IDX_BRIGHTNESS] = {tr(StringId::BRIGHTNESS), 0, false, nullptr};
    s_settingsItems[SETTINGS_IDX_LANGUAGE] = {tr(StringId::LANGUAGE), 0, false, nullptr};
    s_settingsItems[SETTINGS_IDX_TIMEZONE] = {tr(StringId::TIMEZONE), 0, false, nullptr};
    s_settingsItems[SETTINGS_IDX_AUTO_SLEEP] = {tr(StringId::AUTO_SLEEP), 0, false, nullptr};
    s_settingsItems[SETTINGS_IDX_BADGE_TEXT] = {tr(StringId::BADGE_TEXT), 0, false, nullptr};
    s_settingsItems[SETTINGS_IDX_SET_DATE] = {tr(StringId::SET_DATE), 0, false, nullptr};
    s_settingsItems[SETTINGS_IDX_SET_TIME] = {tr(StringId::SET_TIME), 0, false, nullptr};
    s_settingsItems[SETTINGS_IDX_CHANGE_PIN] = {tr(StringId::CHANGE_PIN), 0, false, nullptr};
    s_settingsMenu = new ListView();
    s_settingsMenu->init(tr(StringId::SETTINGS), s_settingsItems, SETTINGS_IDX_COUNT);
    s_settingsMenu->setOnSelect(onSettingsSelect);

    // Initialize settings handlers
    settings::init(s_deps.display, s_deps.sleep, s_lockScreen);

    // Brightness Slider
    s_brightnessSlider = new SliderView();
    uint16_t currentBrightness = s_deps.display ? s_deps.display->getBacklight() / 10 : 50;
    s_brightnessSlider->init(tr(StringId::BRIGHTNESS), 0, 100, currentBrightness, 1, "%");
    s_brightnessSlider->setStepCallback(settings::brightnessStepCallback);
    s_brightnessSlider->setOnSave(settings::onBrightnessSave);
    s_brightnessSlider->setOnChange(settings::onBrightnessChange);

    // Auto Sleep Slider
    s_sleepSlider = new SliderView();
    uint16_t currentSleepMin = 0;
    if (s_deps.sleep) {
        currentSleepMin = static_cast<uint16_t>(s_deps.sleep->getLightSleepInterval() / 60);
    }
    s_sleepSlider->init(tr(StringId::AUTO_SLEEP), 0, 60, currentSleepMin, 1, tr(StringId::MINUTES));
    s_sleepSlider->setZeroLabel(tr(StringId::NEVER));
    s_sleepSlider->setOnSave(settings::onSleepIntervalSave);

    // Timezone Slider
    s_timezoneSlider = new SliderView();
    {
        auto* rtcTz = hal::getRtcInstance();
        int8_t currentTz = rtcTz ? rtcTz->getTimezoneOffset() : 0;
        uint16_t tzSliderValue = static_cast<uint16_t>(currentTz + 12);
        s_timezoneSlider->init(tr(StringId::TIMEZONE), 0, 26, tzSliderValue, 1, "h");
        s_timezoneSlider->setDisplayOffset(-12);
        s_timezoneSlider->setOnSave(settings::onTimezoneSave);
    }

    // Language Menu
    s_languageItems[LANG_IDX_ENGLISH] = {I18n::instance().getLanguageName(Language::EN), 0, false, nullptr};
    s_languageItems[LANG_IDX_GERMAN] = {I18n::instance().getLanguageName(Language::DE), 0, false, nullptr};
    s_languageMenu = new ListView();
    s_languageMenu->init(tr(StringId::LANGUAGE), s_languageItems, LANG_IDX_COUNT);
    s_languageMenu->setOnSelect(onLanguageSelect);

    // Date/Time Input Views
    time_t now = time(nullptr);
    struct tm* tm = localtime(&now);
    s_dateInput = new DateInputView();
    s_dateInput->init(tr(StringId::SET_DATE),
                      tm ? tm->tm_mday : 1,
                      tm ? tm->tm_mon + 1 : 1,
                      tm ? tm->tm_year + 1900 : 2026);
    s_dateInput->setOnConfirm(settings::onDateConfirm);

    s_timeInput = new TimeInputView();
    s_timeInput->init(tr(StringId::SET_TIME),
                      tm ? tm->tm_hour : 12,
                      tm ? tm->tm_min : 0);
    s_timeInput->setOnConfirm(settings::onTimeConfirm);

    // PIN Change View
    s_pinChangeView = new PinChangeView();
    s_pinChangeView->init(core::PinManager::BADGE_PIN_MIN, core::PinManager::BADGE_PIN_MAX);
    s_pinChangeView->setOnComplete(settings::onPinChangeComplete);

    // Initialize PinManager
    core::PinManager::instance().init();

    // Initialize SleepManager
    SleepManager::instance().init(s_deps.sleep, s_deps.power, s_lockScreen);

    // Push to ViewStack
    ViewStack::instance().push(s_lockScreen);

    // Configure inactivity timeout
    ViewStack::instance().setInactivityTimeout(onInactivityTimeout, INACTIVITY_TIMEOUT_MS);

    // Subscribe to module error events
    core::EventBus::instance().subscribe(onModuleErrorEvent, static_cast<uint32_t>(core::EventType::MODULE_ERROR));

    // Serial callbacks
    serial::SerialCmd::setTextCallback([](const char* field, const char* value) {
        if (!s_lockScreen) return;
        if (strcmp(field, "name") == 0) {
            s_lockScreen->setDisplayName(value);
            settings::saveDisplayField("name", value);
        } else if (strcmp(field, "info") == 0) {
            s_lockScreen->setInfo(value);
            settings::saveDisplayField("info", value);
        } else if (strcmp(field, "info2") == 0) {
            s_lockScreen->setInfo2(value);
            settings::saveDisplayField("info2", value);
        }
    });

    serial::SerialCmd::setTimeCallback([]() {
        if (!s_lockScreen) return;
        time_t now = time(nullptr);
        struct tm* tm = localtime(&now);
        if (tm) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%02d:%02d", tm->tm_hour, tm->tm_min);
            s_lockScreen->setClock(buf);
            snprintf(buf, sizeof(buf), "%02d.%02d.%d", tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900);
            s_lockScreen->setDate(buf);
        }
    });
}

void ui_on_modules_ready() {
    rebuildToolsMenu();
    rebuildMainMenu();
}

void ui_rebuild_menus() {
    rebuildToolsMenu();
    rebuildMainMenu();
}

void ui_process(uint32_t nowMs) {
    // Update status icons when on lock screen
    if (s_lockScreen && ViewStack::instance().current() == s_lockScreen) {
        updatePowerStatusIcons();
    }

    // Update clock at minute change
    updateLockScreenClock();

    // Keypad input
    if (s_deps.keypad) {
        if (s_ignoreKeyUntilRelease) {
            clearKeypadBuffer();
            if (!s_deps.keypad->anyKeyDown()) {
                s_ignoreKeyUntilRelease = false;
            }
        } else {
            hal::Key key = s_deps.keypad->getNextKey();
            if (key != hal::Key::KEY_NONE) {
                char keyChar = static_cast<char>(key);
                ViewStack::instance().dispatchKey(keyChar);
                SleepManager::instance().resetTimer(nowMs);
            }
        }
    }

    settings::processPendingBadgeText();

    // Dispatch tick + inactivity
    ViewStack::instance().dispatchTick(nowMs);
    if (ViewStack::instance().depth() > 1) {
        ViewStack::instance().checkInactivity(nowMs);
    } else {
        // On lock screen: check for light sleep
        SleepManager::instance().checkLockScreenSleep(nowMs);
    }

    // Render if needed
    if (ViewStack::instance().needsRender()) {
        ViewStack::instance().render();
    }
}

} // namespace cdc::ui

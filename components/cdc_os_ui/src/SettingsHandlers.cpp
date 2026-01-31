#include "cdc_os_ui/SettingsHandlers.h"
#include "cdc_os_ui/views/LockScreenView.h"
#include "cdc_ui/ViewStack.h"
#include "cdc_ui/I18n.h"
#include "cdc_views/T9InputView.h"
#include "cdc_hal/IDisplay.h"
#include "cdc_hal/ISleepController.h"
#include "cdc_hal/IRtc.h"
#include "nvs.h"
#include <ctime>
#include <sys/time.h>

namespace cdc::ui::settings {

// External dependencies (set by AppUi)
static hal::IDisplay* s_display = nullptr;
static hal::ISleepController* s_sleep = nullptr;
static LockScreenView* s_lockScreen = nullptr;

// Badge text editing state
static constexpr uint8_t BADGE_STEP_NONE = 0;
static constexpr uint8_t BADGE_STEP_NAME = 1;
static constexpr uint8_t BADGE_STEP_INFO = 2;
static constexpr uint8_t BADGE_STEP_INFO2 = 3;
static uint8_t s_badgeTextPendingStep = BADGE_STEP_NONE;

// Forward declarations for internal use
static void showBadgeTextStep(uint8_t step);
static void onBadgeNameSave(const char* text);
static void onBadgeInfoSave(const char* text);
static void onBadgeInfo2Save(const char* text);

// Initialize dependencies (called from AppUi)
void init(hal::IDisplay* display, hal::ISleepController* sleep, LockScreenView* lockScreen) {
    s_display = display;
    s_sleep = sleep;
    s_lockScreen = lockScreen;
}

// Process pending badge text steps (call from ui_process)
void processPendingBadgeText() {
    if (s_badgeTextPendingStep == BADGE_STEP_NONE) return;
    uint8_t step = s_badgeTextPendingStep;
    s_badgeTextPendingStep = BADGE_STEP_NONE;
    showBadgeTextStep(step);
}

void onBrightnessSave(uint16_t value) {
    if (s_display) {
        s_display->setBacklight(value * 10);
        s_display->saveBacklight();
    }
}

void onBrightnessChange(uint16_t value) {
    if (s_display) {
        s_display->setBacklight(value * 10);
    }
}

uint16_t brightnessStepCallback(uint16_t current, bool increasing) {
    if (increasing) {
        if (current < 1) return 1;
        if (current < 20) return 5;
        return 10;
    }
    if (current <= 1) return 1;
    if (current <= 20) return 5;
    return 10;
}

void onSleepIntervalSave(uint16_t value) {
    if (s_sleep) {
        s_sleep->setLightSleepInterval(static_cast<uint32_t>(value) * 60);
    }
}

void onTimezoneSave(uint16_t value) {
    auto* rtc = hal::getRtcInstance();
    if (rtc) {
        // Value is 0-26 (slider range), convert to -12..+14 (actual timezone)
        int8_t tzOffset = static_cast<int8_t>(static_cast<int16_t>(value) - 12);
        rtc->setTimezoneOffset(tzOffset);

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
    }
}

void onDateConfirm(uint8_t day, uint8_t month, uint16_t year) {
    time_t now = time(nullptr);
    struct tm tm = {};
    struct tm* current = localtime(&now);
    if (current) tm = *current;
    tm.tm_mday = day;
    tm.tm_mon = month - 1;
    tm.tm_year = year - 1900;

    time_t newTime = mktime(&tm);
    struct timeval tv = {.tv_sec = newTime, .tv_usec = 0};
    settimeofday(&tv, nullptr);
}

void onTimeConfirm(uint8_t hour, uint8_t minute) {
    time_t now = time(nullptr);
    struct tm tm = {};
    struct tm* current = localtime(&now);
    if (current) tm = *current;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = 0;

    time_t newTime = mktime(&tm);
    struct timeval tv = {.tv_sec = newTime, .tv_usec = 0};
    settimeofday(&tv, nullptr);
}

void onPinChangeComplete(bool success) {
    (void)success;
    ViewStack::instance().pop();
}

void startBadgeTextEdit() {
    showBadgeTextStep(BADGE_STEP_NAME);
}

static void showBadgeTextStep(uint8_t step) {
    if (!s_lockScreen) return;

    const char* title = nullptr;
    const char* initial = nullptr;
    T9InputView::SaveCallback cb = nullptr;

    switch (step) {
        case BADGE_STEP_NAME:
            title = tr(StringId::NAME);
            initial = s_lockScreen->getDisplayName();
            cb = onBadgeNameSave;
            break;
        case BADGE_STEP_INFO:
            title = tr(StringId::INFO);
            initial = s_lockScreen->getInfo();
            cb = onBadgeInfoSave;
            break;
        case BADGE_STEP_INFO2:
            title = tr(StringId::INFO2);
            initial = s_lockScreen->getInfo2();
            cb = onBadgeInfo2Save;
            break;
        default:
            return;
    }

    showT9Input(title, initial, cb, LockScreenView::MAX_TEXT_LEN);
}

static void onBadgeNameSave(const char* text) {
    if (s_lockScreen) s_lockScreen->setDisplayName(text);
    saveDisplayField("name", text);
    s_badgeTextPendingStep = BADGE_STEP_INFO;
}

static void onBadgeInfoSave(const char* text) {
    if (s_lockScreen) s_lockScreen->setInfo(text);
    saveDisplayField("info", text);
    s_badgeTextPendingStep = BADGE_STEP_INFO2;
}

static void onBadgeInfo2Save(const char* text) {
    if (s_lockScreen) s_lockScreen->setInfo2(text);
    saveDisplayField("info2", text);
    s_badgeTextPendingStep = BADGE_STEP_NONE;
}

void saveDisplayField(const char* key, const char* value) {
    if (!key) return;
    nvs_handle_t nvs;
    if (nvs_open("display", NVS_READWRITE, &nvs) != ESP_OK) return;
    const char* safeValue = value ? value : "";
    nvs_set_str(nvs, key, safeValue);
    nvs_commit(nvs);
    nvs_close(nvs);
}

} // namespace cdc::ui::settings

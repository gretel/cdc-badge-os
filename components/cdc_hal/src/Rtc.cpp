/**
 * ESP32-S3 Internal RTC Implementation
 *
 * Uses the ESP32-S3's internal RTC. Time survives light sleep but NOT
 * deep sleep or power cycles. Time is NOT persisted to NVS - better no
 * time than a wrong/stale time.
 *
 * Based on: ~/GIT/cdc-badge-os-legacy/components/cdc_badge/cdc_rtc.cpp
 */

#include "cdc_hal/IRtc.h"
#include "cdc_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <sys/time.h>
#include <cstring>

static const char* TAG = "RTC";

namespace cdc::hal {

/**
 * \brief NVS namespace used to persist timezone metadata (not wall-clock time).
 */
static constexpr const char* NVS_NAMESPACE = "rtc";
static constexpr const char* NVS_KEY_TZ = "tz_offset";

class Esp32Rtc : public IRtc {
public:
    Esp32Rtc() = default;

    // IService implementation
    bool init() override;
    bool start() override { state_ = core::ServiceState::STARTED; return true; }
    void stop() override { state_ = core::ServiceState::STOPPED; }
    core::ServiceState getState() const override { return state_; }
    const char* getName() const override { return "rtc"; }

    // IRtc implementation
    void getTime(struct tm* timeinfo) const override;
    void getTimeStr(char* buf, size_t bufLen) const override;
    void getDateStr(char* buf, size_t bufLen) const override;
    void setTime(int hour, int minute, int second) override;
    void setDate(int year, int month, int day) override;
    void setTimestamp(time_t timestamp) override;
    time_t getTimestamp() const override;
    bool isTimeSet() const override { return timeIsSet_; }
    void markTimeSet() override { timeIsSet_ = true; }
    void setTimezoneOffset(int8_t hours) override;
    int8_t getTimezoneOffset() const override { return tzOffset_; }

private:
    void loadTimezoneFromNvs();
    void applyTimezone();

    core::ServiceState state_ = core::ServiceState::UNINITIALIZED;
    bool timeIsSet_ = false;
    int8_t tzOffset_ = 0;  // Hours from UTC
};

/**
 * \brief Initializes RTC service and timezone context.
 * \return `true` if initialization succeeded.
 */
bool Esp32Rtc::init() {
    if (state_ != core::ServiceState::UNINITIALIZED) {
        return state_ == core::ServiceState::INITIALIZED ||
               state_ == core::ServiceState::STARTED;
    }

    LOG_I(TAG, "Initializing ESP32-S3 internal RTC");

    // Load timezone from NVS (timezone is persisted, time is not)
    loadTimezoneFromNvs();
    applyTimezone();

    // Check if internal RTC has valid time (year >= 2024)
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    LOG_I(TAG, "Internal RTC: %04d-%02d-%02d %02d:%02d (ts=%ld)",
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, (long)now);

    if (timeinfo.tm_year >= (2024 - 1900)) {
        timeIsSet_ = true;
        LOG_I(TAG, "RTC time valid");
    } else {
        // Time not valid - user must sync via NTP or SET_TIME
        // We do NOT restore from NVS - better no time than stale time
        timeIsSet_ = false;
        LOG_I(TAG, "RTC time not set - use NTP or SET_TIME command");
    }

    state_ = core::ServiceState::INITIALIZED;
    return true;
}

/**
 * \brief Reads current local time into `tm`.
 * \param timeinfo Output time structure.
 * \return void
 */
void Esp32Rtc::getTime(struct tm* timeinfo) const {
    if (!timeinfo) return;
    time_t now;
    time(&now);
    localtime_r(&now, timeinfo);
}

/**
 * \brief Formats current time as `HH:MM`.
 * \param buf Output character buffer.
 * \param bufLen Size of output buffer.
 * \return void
 */
void Esp32Rtc::getTimeStr(char* buf, size_t bufLen) const {
    if (!buf || bufLen < 6) return;
    struct tm timeinfo;
    getTime(&timeinfo);
    strftime(buf, bufLen, "%H:%M", &timeinfo);
}

/**
 * \brief Formats current date as `YYYY-MM-DD`.
 * \param buf Output character buffer.
 * \param bufLen Size of output buffer.
 * \return void
 */
void Esp32Rtc::getDateStr(char* buf, size_t bufLen) const {
    if (!buf || bufLen < 11) return;
    struct tm timeinfo;
    getTime(&timeinfo);
    strftime(buf, bufLen, "%Y-%m-%d", &timeinfo);
}

/**
 * \brief Sets RTC time while preserving current date.
 * \param hour Hour value.
 * \param minute Minute value.
 * \param second Second value.
 * \return void
 */
void Esp32Rtc::setTime(int hour, int minute, int second) {
    struct tm timeinfo;
    getTime(&timeinfo);

    // If year is invalid, set a default date
    if (timeinfo.tm_year < (2024 - 1900)) {
        LOG_W(TAG, "Year invalid (%d), setting default date 2025-01-01",
                 timeinfo.tm_year + 1900);
        timeinfo.tm_year = 2025 - 1900;
        timeinfo.tm_mon = 0;   // January
        timeinfo.tm_mday = 1;
    }

    timeinfo.tm_hour = hour;
    timeinfo.tm_min = minute;
    timeinfo.tm_sec = second;

    time_t t = mktime(&timeinfo);
    struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
    settimeofday(&tv, nullptr);

    timeIsSet_ = true;

    LOG_I(TAG, "Time set to %02d:%02d:%02d (date: %04d-%02d-%02d)",
             hour, minute, second,
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
}

/**
 * \brief Sets RTC date while preserving current time.
 * \param year Year value.
 * \param month Month value.
 * \param day Day value.
 * \return void
 */
void Esp32Rtc::setDate(int year, int month, int day) {
    struct tm timeinfo;
    getTime(&timeinfo);

    timeinfo.tm_year = year - 1900;
    timeinfo.tm_mon = month - 1;
    timeinfo.tm_mday = day;

    time_t t = mktime(&timeinfo);
    struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
    settimeofday(&tv, nullptr);

    timeIsSet_ = true;

    LOG_I(TAG, "Date set to %04d-%02d-%02d", year, month, day);
}

/**
 * \brief Sets RTC from UNIX timestamp.
 * \param timestamp UNIX timestamp.
 * \return void
 */
void Esp32Rtc::setTimestamp(time_t timestamp) {
    struct timeval tv = { .tv_sec = timestamp, .tv_usec = 0 };
    settimeofday(&tv, nullptr);

    timeIsSet_ = true;

    struct tm timeinfo;
    localtime_r(&timestamp, &timeinfo);
    LOG_I(TAG, "Timestamp set: %04d-%02d-%02d %02d:%02d:%02d",
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
}

/**
 * \brief Returns current UNIX timestamp.
 * \return Current UNIX timestamp.
 */
time_t Esp32Rtc::getTimestamp() const {
    time_t now;
    time(&now);
    return now;
}

/**
 * \brief Sets timezone offset and persists it.
 * \param hours Timezone offset from UTC.
 * \return void
 */
void Esp32Rtc::setTimezoneOffset(int8_t hours) {
    if (hours < -12 || hours > 14) {
        LOG_W(TAG, "Invalid timezone offset: %d", hours);
        return;
    }

    tzOffset_ = hours;
    applyTimezone();

    // Save to NVS
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_i8(nvs, NVS_KEY_TZ, tzOffset_);
        nvs_commit(nvs);
        nvs_close(nvs);
    }

    LOG_I(TAG, "Timezone set to UTC%+d", hours);
}

/**
 * \brief Loads timezone offset from NVS.
 * \return void
 */
void Esp32Rtc::loadTimezoneFromNvs() {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) == ESP_OK) {
        int8_t tz = 0;
        if (nvs_get_i8(nvs, NVS_KEY_TZ, &tz) == ESP_OK) {
            tzOffset_ = tz;
        }
        nvs_close(nvs);
    }
}

/**
 * \brief Applies timezone offset to C runtime environment.
 * \return void
 */
void Esp32Rtc::applyTimezone() {
    // Set TZ environment variable
    // Format: "UTC<offset>" where offset is negated (UTC-1 means TZ=UTC1)
    char tz[16];
    if (tzOffset_ == 0) {
        snprintf(tz, sizeof(tz), "UTC0");
    } else {
        snprintf(tz, sizeof(tz), "UTC%+d", -tzOffset_);
    }
    setenv("TZ", tz, 1);
    tzset();
}

/**
 * \brief Singleton RTC implementation instance.
 */
static Esp32Rtc g_rtc;

/**
 * \brief Returns the singleton RTC service instance.
 * \return Pointer to the global `IRtc` implementation.
 */
IRtc* getRtcInstance() {
    return &g_rtc;
}

} // namespace cdc::hal

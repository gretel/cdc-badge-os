// RTC module for CDC Badge
// Uses ESP32's internal RTC
// Time is persisted to NVS to survive resets

#include "cdc_rtc.h"
#include "cdc_log.h"
#include <sys/time.h>
#include <string.h>
#include "nvs_flash.h"
#include "nvs.h"

static bool time_is_set = false;

#define NVS_NAMESPACE "rtc"
#define NVS_KEY_TIME "timestamp"

// Save current time to NVS
static void save_time_to_nvs(void) {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        time_t now;
        time(&now);
        nvs_set_i64(nvs, NVS_KEY_TIME, (int64_t)now);
        nvs_commit(nvs);
        nvs_close(nvs);
    }
}

void cdc_rtc_init(void) {
    LOG_I("RTC", "Initializing RTC");

    // Set timezone to UTC (can be changed later)
    setenv("TZ", "UTC0", 1);
    tzset();

    // Check internal RTC value
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    LOG_I("RTC", "Internal RTC: %04d-%02d-%02d %02d:%02d (ts=%ld)",
          timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
          timeinfo.tm_hour, timeinfo.tm_min, (long)now);

    // Only use time if internal RTC has valid time (year >= 2024)
    // Do NOT restore from NVS - better no time than wrong time
    if (timeinfo.tm_year >= (2024 - 1900)) {
        time_is_set = true;
        LOG_I("RTC", "RTC time valid");
    } else {
        time_is_set = false;
        LOG_I("RTC", "RTC time not set - use TIME_SET command");
    }
}

void cdc_rtc_get_time(struct tm *timeinfo) {
    if (!timeinfo) return;

    time_t now;
    time(&now);
    localtime_r(&now, timeinfo);
}

void cdc_rtc_get_time_str(char *buf, size_t buf_len) {
    if (!buf || buf_len < 6) return;

    struct tm timeinfo;
    cdc_rtc_get_time(&timeinfo);
    strftime(buf, buf_len, "%H:%M", &timeinfo);
}

void cdc_rtc_get_date_str(char *buf, size_t buf_len) {
    if (!buf || buf_len < 11) return;

    struct tm timeinfo;
    cdc_rtc_get_time(&timeinfo);
    strftime(buf, buf_len, "%Y-%m-%d", &timeinfo);
}

void cdc_rtc_set_time(int hour, int minute, int second) {
    struct tm timeinfo;
    cdc_rtc_get_time(&timeinfo);

    // If year is invalid (< 2024), set a default date
    if (timeinfo.tm_year < (2024 - 1900)) {
        LOG_W("RTC", "Year invalid (%d), setting default date 2025-01-01",
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
    settimeofday(&tv, NULL);

    time_is_set = true;
    save_time_to_nvs();
    LOG_I("RTC", "Time set to %02d:%02d:%02d (date: %04d-%02d-%02d)",
          hour, minute, second,
          timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
}

void cdc_rtc_set_date(int year, int month, int day) {
    struct tm timeinfo;
    cdc_rtc_get_time(&timeinfo);

    timeinfo.tm_year = year - 1900;
    timeinfo.tm_mon = month - 1;
    timeinfo.tm_mday = day;

    time_t t = mktime(&timeinfo);
    struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
    settimeofday(&tv, NULL);

    time_is_set = true;
    save_time_to_nvs();
    LOG_I("RTC", "Date set to %04d-%02d-%02d", year, month, day);
}

bool cdc_rtc_is_time_set(void) {
    return time_is_set;
}

void cdc_rtc_mark_time_set(void) {
    time_is_set = true;
    save_time_to_nvs();
}

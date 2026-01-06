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

// Load time from NVS
static bool load_time_from_nvs(void) {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) == ESP_OK) {
        int64_t saved_time = 0;
        if (nvs_get_i64(nvs, NVS_KEY_TIME, &saved_time) == ESP_OK) {
            nvs_close(nvs);
            // Validate: time should be after 2024
            if (saved_time > 1704067200) {  // 2024-01-01 00:00:00 UTC
                struct timeval tv = { .tv_sec = (time_t)saved_time, .tv_usec = 0 };
                settimeofday(&tv, NULL);
                LOG_I("RTC", "Restored time from NVS: %lld", saved_time);
                return true;
            }
        }
        nvs_close(nvs);
    }
    return false;
}

void cdc_rtc_init(void) {
    LOG_I("RTC", "Initializing RTC");

    // Set timezone to UTC (can be changed later)
    setenv("TZ", "UTC0", 1);
    tzset();

    // Check internal RTC value first
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    LOG_I("RTC", "Internal RTC: %04d-%02d-%02d %02d:%02d (ts=%ld)",
          timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
          timeinfo.tm_hour, timeinfo.tm_min, (long)now);

    // If RTC has valid time (year >= 2024), use it
    if (timeinfo.tm_year >= (2024 - 1900)) {
        time_is_set = true;
        LOG_I("RTC", "RTC time valid");
    }
    // Otherwise try NVS backup
    else if (load_time_from_nvs()) {
        time_is_set = true;
        time(&now);
        localtime_r(&now, &timeinfo);
        LOG_I("RTC", "RTC restored from NVS: %04d-%02d-%02d %02d:%02d",
              timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
              timeinfo.tm_hour, timeinfo.tm_min);
    } else {
        time_is_set = false;
        LOG_I("RTC", "RTC time not set");
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

    timeinfo.tm_hour = hour;
    timeinfo.tm_min = minute;
    timeinfo.tm_sec = second;

    time_t t = mktime(&timeinfo);
    struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
    settimeofday(&tv, NULL);

    time_is_set = true;
    save_time_to_nvs();
    LOG_I("RTC", "Time set to %02d:%02d:%02d", hour, minute, second);
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

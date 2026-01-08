// NTP Sync Component
// Synchronizes time from NTP server

#include "ntp_sync.h"
#include "cdc_rtc.h"
#include "badge_settings.h"
#include "cdc_log.h"

#include <esp_sntp.h>
#include <esp_timer.h>
#include <time.h>
#include <string.h>

#define TAG "NTP"
#define NTP_TIMEOUT_MS 10000

static ntp_state_t g_ntp_state = NTP_STATE_IDLE;
static uint32_t g_start_time = 0;

// SNTP time sync callback
static void ntp_time_sync_cb(struct timeval *tv) {
    if (tv == nullptr) {
        LOG_E(TAG, "Time sync failed (null timeval)");
        g_ntp_state = NTP_STATE_FAILED;
        return;
    }

    // Get synchronized time
    time_t now = tv->tv_sec;
    struct tm timeinfo;

    // Apply timezone offset
    int8_t tz_offset = badge_settings_get_timezone();

    // Create timezone string (e.g., "UTC-1" for UTC+1, because POSIX is inverted)
    char tz_str[16];
    if (tz_offset >= 0) {
        snprintf(tz_str, sizeof(tz_str), "UTC-%d", tz_offset);
    } else {
        snprintf(tz_str, sizeof(tz_str), "UTC+%d", -tz_offset);
    }
    setenv("TZ", tz_str, 1);
    tzset();

    localtime_r(&now, &timeinfo);

    // Set RTC
    cdc_rtc_set_date(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
    cdc_rtc_set_time(timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    g_ntp_state = NTP_STATE_SUCCESS;
    LOG_I(TAG, "Time synced: %04d-%02d-%02d %02d:%02d:%02d (TZ: UTC%+d)",
          timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
          timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, tz_offset);
}

void ntp_sync_start(uint32_t dhcp_ntp_server) {
    if (g_ntp_state == NTP_STATE_SYNCING) {
        LOG_W(TAG, "NTP sync already in progress");
        return;
    }

    g_ntp_state = NTP_STATE_SYNCING;
    g_start_time = (uint32_t)(esp_timer_get_time() / 1000);

    // Stop any previous SNTP instance
    if (esp_sntp_enabled()) {
        esp_sntp_stop();
    }

    // Configure SNTP
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);

    // Use DHCP-provided NTP server if available
    if (dhcp_ntp_server != 0) {
        char ntp_ip[16];
        snprintf(ntp_ip, sizeof(ntp_ip), "%u.%u.%u.%u",
                 (unsigned)(dhcp_ntp_server & 0xFF),
                 (unsigned)((dhcp_ntp_server >> 8) & 0xFF),
                 (unsigned)((dhcp_ntp_server >> 16) & 0xFF),
                 (unsigned)((dhcp_ntp_server >> 24) & 0xFF));

        LOG_I(TAG, "Using DHCP NTP server: %s", ntp_ip);
        esp_sntp_setservername(0, ntp_ip);
        esp_sntp_setservername(1, "pool.ntp.org");  // Fallback
    } else {
        LOG_I(TAG, "Using default NTP servers");
        esp_sntp_setservername(0, "pool.ntp.org");
        esp_sntp_setservername(1, "time.google.com");
    }

    // Set callback
    esp_sntp_set_time_sync_notification_cb(ntp_time_sync_cb);

    // Start SNTP
    esp_sntp_init();
    LOG_I(TAG, "SNTP started");
}

ntp_state_t ntp_sync_get_state(void) {
    return g_ntp_state;
}

void ntp_sync_stop(void) {
    if (esp_sntp_enabled()) {
        esp_sntp_stop();
        LOG_I(TAG, "SNTP stopped");
    }
    g_ntp_state = NTP_STATE_IDLE;
}

bool ntp_sync_timed_out(void) {
    if (g_ntp_state != NTP_STATE_SYNCING) {
        return false;
    }

    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    return (now - g_start_time) > NTP_TIMEOUT_MS;
}

#pragma once

// RTC module for CDC Badge
// Uses ESP32's internal RTC

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize RTC (sets timezone to UTC)
void cdc_rtc_init(void);

// Get current time as struct tm
void cdc_rtc_get_time(struct tm *timeinfo);

// Get current time as formatted string (HH:MM)
void cdc_rtc_get_time_str(char *buf, size_t buf_len);

// Get current date as formatted string (YYYY-MM-DD)
void cdc_rtc_get_date_str(char *buf, size_t buf_len);

// Set time manually (hour, minute, second)
void cdc_rtc_set_time(int hour, int minute, int second);

// Set date manually (year, month 1-12, day 1-31)
void cdc_rtc_set_date(int year, int month, int day);

// Check if time has been set (either manually or via NTP)
bool cdc_rtc_is_time_set(void);

// Mark time as set (call after settimeofday from external code)
void cdc_rtc_mark_time_set(void);

#ifdef __cplusplus
}
#endif

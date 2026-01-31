#pragma once

#include "cdc_core/IService.h"
#include <cstdint>
#include <ctime>

namespace cdc::hal {

/**
 * RTC (Real-Time Clock) Interface
 *
 * Uses ESP32-S3's internal RTC. Time survives light sleep but NOT deep sleep
 * or power cycles without battery backup.
 *
 * Reference: ~/GIT/cdc-badge-os-legacy/components/cdc_badge/cdc_rtc.cpp
 */
class IRtc : public core::IService {
public:
    virtual ~IRtc() = default;

    /**
     * Get current time as struct tm
     */
    virtual void getTime(struct tm* timeinfo) const = 0;

    /**
     * Get time as formatted string "HH:MM"
     */
    virtual void getTimeStr(char* buf, size_t bufLen) const = 0;

    /**
     * Get date as formatted string "YYYY-MM-DD"
     */
    virtual void getDateStr(char* buf, size_t bufLen) const = 0;

    /**
     * Set time (hour, minute, second)
     * If date is not set, defaults to 2025-01-01
     */
    virtual void setTime(int hour, int minute, int second) = 0;

    /**
     * Set date (year, month 1-12, day 1-31)
     */
    virtual void setDate(int year, int month, int day) = 0;

    /**
     * Set time from Unix timestamp
     */
    virtual void setTimestamp(time_t timestamp) = 0;

    /**
     * Get Unix timestamp
     */
    virtual time_t getTimestamp() const = 0;

    /**
     * Check if time has been set (year >= 2024)
     */
    virtual bool isTimeSet() const = 0;

    /**
     * Mark time as set (e.g. after NTP sync)
     */
    virtual void markTimeSet() = 0;

    /**
     * Set timezone offset in hours from UTC
     * E.g., +1 for CET, +2 for CEST
     */
    virtual void setTimezoneOffset(int8_t hours) = 0;

    /**
     * Get timezone offset in hours
     */
    virtual int8_t getTimezoneOffset() const = 0;
};

// Factory function
IRtc* getRtcInstance();

} // namespace cdc::hal

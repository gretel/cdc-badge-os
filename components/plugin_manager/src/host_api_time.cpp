/**
 * \file host_api_time.cpp
 * \brief Real implementations of the time-related host API functions.
 */

#include "cdc_hal/IRtc.h"
#include "plugin_manager/host_api.h"
#include "esp_timer.h"

#include <ctime>

using cdc::hal::IRtc;
using cdc::hal::getRtcInstance;

extern "C" {

uint64_t host_uptime_ms(void)
{
    return static_cast<uint64_t>(esp_timer_get_time() / 1000LL);
}

int64_t host_unix_time(void)
{
    auto* r = getRtcInstance();
    return r ? static_cast<int64_t>(r->getTimestamp()) : 0;
}

bool host_is_time_set(void)
{
    auto* r = getRtcInstance();
    return r ? r->isTimeSet() : false;
}

int32_t host_timezone_offset(void)
{
    auto* r = getRtcInstance();
    return r ? r->getTimezoneOffset() : 0;
}

int host_local_time(struct host_tm* out)
{
    if (!out) return HOST_ERR_INVALID_ARG;
    auto* r = getRtcInstance();
    if (!r) return HOST_ERR_NOT_FOUND;
    std::time_t ts = static_cast<std::time_t>(r->getTimestamp());
    std::tm tm{};
    std::tm* local = std::localtime(&ts);
    if (local) tm = *local;
    out->year    = static_cast<uint16_t>(tm.tm_year + 1900);
    out->month   = static_cast<uint8_t>(tm.tm_mon + 1);
    out->day     = static_cast<uint8_t>(tm.tm_mday);
    out->hour    = static_cast<uint8_t>(tm.tm_hour);
    out->minute  = static_cast<uint8_t>(tm.tm_min);
    out->second  = static_cast<uint8_t>(tm.tm_sec);
    out->weekday = static_cast<uint8_t>(tm.tm_wday);
    return HOST_OK;
}

}  // extern "C"

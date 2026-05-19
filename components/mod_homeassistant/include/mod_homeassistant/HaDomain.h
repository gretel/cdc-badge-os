#pragma once

#include <cstdint>
#include <cstring>

namespace cdc::mod_homeassistant {

/**
 * \brief Supported Home Assistant entity domains (V1 scope).
 *
 * Numeric values are persisted in NVS-stored favorites and must remain stable
 * across builds. Append new domains, do not reorder.
 */
enum class HaDomain : uint8_t {
    LIGHT          = 0,
    SWITCH         = 1,
    INPUT_BOOLEAN  = 2,
    SCENE          = 3,
    SCRIPT         = 4,
    AUTOMATION     = 5,
    SENSOR         = 6,
    BINARY_SENSOR  = 7,
    COVER          = 8,
    CLIMATE        = 9,
    UNKNOWN        = 0xFF,
};

inline HaDomain parseDomain(const char* entityId) {
    if (!entityId) return HaDomain::UNKNOWN;
    if (strncmp(entityId, "light.",         6)  == 0) return HaDomain::LIGHT;
    if (strncmp(entityId, "switch.",        7)  == 0) return HaDomain::SWITCH;
    if (strncmp(entityId, "input_boolean.", 14) == 0) return HaDomain::INPUT_BOOLEAN;
    if (strncmp(entityId, "scene.",         6)  == 0) return HaDomain::SCENE;
    if (strncmp(entityId, "script.",        7)  == 0) return HaDomain::SCRIPT;
    if (strncmp(entityId, "automation.",    11) == 0) return HaDomain::AUTOMATION;
    if (strncmp(entityId, "binary_sensor.", 14) == 0) return HaDomain::BINARY_SENSOR;
    if (strncmp(entityId, "sensor.",        7)  == 0) return HaDomain::SENSOR;
    if (strncmp(entityId, "cover.",         6)  == 0) return HaDomain::COVER;
    if (strncmp(entityId, "climate.",       8)  == 0) return HaDomain::CLIMATE;
    return HaDomain::UNKNOWN;
}

inline bool isReadOnly(HaDomain d) {
    return d == HaDomain::SENSOR || d == HaDomain::BINARY_SENSOR;
}

inline bool isToggleable(HaDomain d) {
    return d == HaDomain::LIGHT
        || d == HaDomain::SWITCH
        || d == HaDomain::INPUT_BOOLEAN;
}

inline bool hasBrightness(HaDomain d) {
    return d == HaDomain::LIGHT;
}

inline const char* getPrimaryService(HaDomain d) {
    switch (d) {
        case HaDomain::LIGHT:
        case HaDomain::SWITCH:
        case HaDomain::INPUT_BOOLEAN:  return "toggle";
        case HaDomain::SCENE:
        case HaDomain::SCRIPT:
        case HaDomain::AUTOMATION:     return "turn_on";
        default:                       return nullptr;
    }
}

inline const char* getDomainString(HaDomain d) {
    switch (d) {
        case HaDomain::LIGHT:          return "light";
        case HaDomain::SWITCH:         return "switch";
        case HaDomain::INPUT_BOOLEAN:  return "input_boolean";
        case HaDomain::SCENE:          return "scene";
        case HaDomain::SCRIPT:         return "script";
        case HaDomain::AUTOMATION:     return "automation";
        case HaDomain::SENSOR:         return "sensor";
        case HaDomain::BINARY_SENSOR:  return "binary_sensor";
        case HaDomain::COVER:          return "cover";
        case HaDomain::CLIMATE:        return "climate";
        default:                       return nullptr;
    }
}

} // namespace cdc::mod_homeassistant

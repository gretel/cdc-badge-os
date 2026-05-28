/**
 * \file PluginManifest.h
 * \brief In-memory representation of a plugin's meta.json.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>

namespace cdc::plugin_manager {

struct PrerequisiteSpec {
    std::string name;
    std::map<std::string, std::string> params;
    std::string on_fail;
};

struct PluginCapabilities {
    bool wifi = false;
    bool ble = false;
    bool http = false;
    bool ui_exclusive = false;
    bool display_lowlevel = false;
    bool sao = false;
    bool grove = false;
    bool pixel_strip = false;
    bool background = false;

    std::vector<std::string> rmem;
    std::vector<uint8_t> ecc_slots;
    std::vector<std::string> ble_service_uuids;
    std::vector<uint8_t> gpio_pins;
    std::vector<uint8_t> pwm_pins;
    std::vector<uint8_t> adc_pins;
    std::vector<uint8_t> i2c_bus;
    std::string nvs_namespace;
};

struct LocalizedString {
    std::map<std::string, std::string> by_lang;
};

struct PluginManifest {
    std::string id;
    std::string version;
    std::string author;
    std::string icon;
    std::string host_api_level_min;
    uint16_t    api_level_major = 0;
    uint16_t    api_level_minor = 0;
    uint32_t    linear_memory_kb = 64;
    std::string default_language;
    std::map<std::string, LocalizedString> i18n_meta;
    std::map<std::string, LocalizedString> i18n_strings;
    PluginCapabilities capabilities;
    std::vector<PrerequisiteSpec> prerequisites;

    /**
     * \brief Parse `meta.json` content. Returns false on schema errors.
     */
    [[nodiscard]] static bool parse(const char* json, size_t len, PluginManifest& out);
};

}  // namespace cdc::plugin_manager

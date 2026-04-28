---
title: "[LOW] Temperature displayed in Celsius only, no locale-aware unit selection"
severity: LOW
domain: i18n
lens: i18n-formatting
labels:
  - audit:i18n/i18n-formatting
---

## Summary

Temperature values are displayed **exclusively in Celsius** (°C) without offering users the option to view Fahrenheit (°F), which is the preferred unit in the United States and a few other countries.

### Files Affected

| File | Lines | Description |
|------|-------|-------------|
| `components/cdc_os_ui/src/HardwareInfo.cpp` | 136-139 | Hardware info temperature display |
| `components/CalEPD/models/plasticlogic/plasticlogic.cpp` | 68-79 | E-paper temperature string formatting |

### Evidence

**HardwareInfo.cpp:136-137** - Celsius hardcoded:
```cpp
if (espHw && espHw->getTemperatureC(&tempC)) {
    append("%s: %.1f C\n", tr(StringId::HW_TEMP), tempC);
```

**plasticlogic.cpp:74-79** - Has Fahrenheit conversion but not exposed to UI:
```cpp
std::string PlasticLogic::readTemperatureString(char type) {
    uint8_t temperature = readTemperature();
    std::string temp = std::to_string(temperature);

    if (type == 'c') {
      temp = temp + " °C";
    } else {
      uint8_t fahrenheit = temperature * 9/5 + 32;
      temp += " °F";
    }
    return temp;
}
```

Note: The CalEPD component has Fahrenheit conversion built-in (for `type != 'c'`), but the HardwareInfo UI only displays Celsius.

## Impact

1. **US Users**: Approximately 330 million people in the US are accustomed to Fahrenheit. Seeing Celsius may be confusing without conversion reference.

2. **Consistency**: The application already supports language localization (English/German), but lacks unit localization for temperature.

3. **Low Priority**: Since this is firmware for a hardware security key, temperature display is primarily for debugging/hardware monitoring rather than user-facing information.

## Recommended Fix

### Add Temperature Unit Preference

1. **Add string IDs for unit labels**:
```cpp
REG(TEMP_CELSIUS,   "°C",        "°C");
REG(TEMP_FAHRENHEIT,"°F",        "°F");
```

2. **Add NVS preference for temperature unit**:
```cpp
static constexpr const char* NVS_UNIT_TEMP = "temp_unit";  // 'C' or 'F'

static char getTemperatureUnit() {
    nvs_handle_t handle;
    if (nvs_open("prefs", NVS_READONLY, &handle) == ESP_OK) {
        char unit = 'C';  // Default
        if (nvs_get_str(handle, NVS_UNIT_TEMP, &unit) == ESP_OK) {
            nvs_close(handle);
            return unit;
        }
    }
    return 'C';  // Default to Celsius
}

static void setTemperatureUnit(char unit) {
    nvs_handle_t handle;
    if (nvs_open("prefs", NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_str(handle, NVS_UNIT_TEMP, &unit);
        nvs_commit(handle);
        nvs_close(handle);
    }
}
```

3. **Update HardwareInfo.cpp to use preference**:
```cpp
if (espHw && espHw->getTemperatureC(&tempC)) {
    char unit = getTemperatureUnit();
    float tempDisplay = tempC;
    if (unit == 'F') {
        tempDisplay = tempC * 9.0f / 5.0f + 32.0f;
    }
    append("%s: %.1f %s\n", tr(StringId::HW_TEMP), tempDisplay,
           unit == 'F' ? tr(StringId::TEMP_FAHRENHEIT) : tr(StringId::TEMP_CELSIUS));
}
```

4. **Optional: Add settings menu option** to change temperature unit (similar to timezone setting).

### Alternative (Simpler) Fix

Since this is primarily for hardware debugging, consider:
- **Keep Celsius only** (most common globally)
- **Add Fahrenheit in parentheses** for clarity: `Temp: 25.0 °C (77 °F)`

## References

- [Temperature Units by Country](https://en.wikipedia.org/wiki/Celsius#Relationship_to_Fahrenheit)
- Most of the world uses Celsius; only US, Burma, and Canada (partially) use Fahrenheit as primary unit

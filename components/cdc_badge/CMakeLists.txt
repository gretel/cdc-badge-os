idf_component_register(
    SRCS "tropic01.cpp" "tropic01_cache.cpp" "spi_bus.cpp" "libtropic_port_esp32.cpp"
         "i2c_bus.cpp" "power_management.cpp" "pin_expander.cpp"
         "cdc_rtc.cpp" "gui.cpp" "views.cpp"
         "pin_storage.cpp" "badge_settings.cpp" "i18n.cpp"
    INCLUDE_DIRS "."
    REQUIRES cdc_utils libtropic_sdk freertos mbedtls driver esp_timer esp_netif nvs_flash CalEPD Adafruit-GFX
)

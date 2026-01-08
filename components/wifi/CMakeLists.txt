idf_component_register(
    SRCS "wifi_manager.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        cdc_badge
        esp_wifi
        esp_netif
        esp_event
        nvs_flash
        freertos
        lwip
)

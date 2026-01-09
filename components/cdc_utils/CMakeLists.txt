idf_component_register(
    SRCS "cdc_log.cpp"
    INCLUDE_DIRS "include"
    REQUIRES freertos esp_timer espressif__tinyusb
)

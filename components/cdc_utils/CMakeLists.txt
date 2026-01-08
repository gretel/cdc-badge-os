idf_component_register(
    SRCS "cdc_log.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos esp_timer espressif__tinyusb
)

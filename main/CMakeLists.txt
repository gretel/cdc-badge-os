idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES cdc_badge cdc_utils nvs_flash freertos esp_timer
)

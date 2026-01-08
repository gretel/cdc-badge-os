idf_component_register(
    SRCS "ntp_sync.cpp"
    INCLUDE_DIRS "."
    REQUIRES cdc_badge esp_timer
)

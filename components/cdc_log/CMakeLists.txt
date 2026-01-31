# CDC Log - Logging over TinyUSB CDC + UART
idf_component_register(
    SRCS
        "src/cdc_log.cpp"
    INCLUDE_DIRS
        "include"
    REQUIRES
        usb_badge
        esp_timer
)

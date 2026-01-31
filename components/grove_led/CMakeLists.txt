# Grove LED Module - WS2813 LED strip control via Grove port

idf_component_register(
    SRCS
        "src/GroveLedModule.cpp"
        "src/RgbInputView.cpp"
    INCLUDE_DIRS
        "include"
    REQUIRES
        cdc_core
        cdc_ui
        cdc_views
        cdc_hal
        cdc_log
        led_strip
        nvs_flash
        freertos
        CalEPD
)

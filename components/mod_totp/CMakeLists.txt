# TOTP Module

idf_component_register(
    SRCS
        "src/TotpModule.cpp"
        "src/TotpStore.cpp"
    INCLUDE_DIRS
        "include"
        "${CMAKE_SOURCE_DIR}/components/serial_cmd/include"
    REQUIRES
        cdc_core
        cdc_ui
        cdc_views
        cdc_hal
        serial_cmd
        cdc_log
        mbedtls
        nvs_flash
        freertos
)

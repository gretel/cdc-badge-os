# Password Module

idf_component_register(
    SRCS
        "src/PasswordModule.cpp"
        "src/PasswordStore.cpp"
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
        nvs_flash
        freertos
)

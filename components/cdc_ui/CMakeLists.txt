# CDC UI Framework - Core UI infrastructure (I18n, ViewStack, IView)

idf_component_register(
    SRCS
        "src/I18n.cpp"
        "src/ViewStack.cpp"
    INCLUDE_DIRS
        "include"
    REQUIRES
        cdc_core
        cdc_hal
        cdc_log
        nvs_flash
        esp_timer
        freertos
        json
        fatfs
)

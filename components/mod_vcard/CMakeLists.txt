# vCard + Beacon Module

idf_component_register(
    SRCS
        "src/VcardModule.cpp"
        "src/VcardWizard.cpp"
        "src/vcard_store.cpp"
        "src/ble_vcard.cpp"
    INCLUDE_DIRS
        "include"
    REQUIRES
        cdc_core
        cdc_ui
        cdc_views
        cdc_log
        nvs_flash
        cdc_hal
        serial_cmd
        freertos
)

# BLE Serial Module - Serial commands over Bluetooth Low Energy

idf_component_register(
    SRCS
        "src/BleSerialModule.cpp"
        "src/BleUartService.cpp"
    INCLUDE_DIRS
        "include"
    REQUIRES
        cdc_core
        cdc_ui
        cdc_views
        cdc_hal
        cdc_log
        serial_cmd
        nvs_flash
        freertos
        bt
)

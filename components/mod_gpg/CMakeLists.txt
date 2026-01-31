# GPG Module (OpenPGP + CCID)

idf_component_register(
    SRCS
        "src/GpgModule.cpp"
        "src/ccid/ccid_driver.cpp"
        "src/gpg.cpp"
        "src/GpgStorage.cpp"
        "src/pin_storage.cpp"
        "src/openpgp/apdu.cpp"
        "src/openpgp/ccid.cpp"
        "src/openpgp/openpgp.cpp"
    INCLUDE_DIRS
        "include"
        "${CMAKE_SOURCE_DIR}/components/serial_cmd/include"
    REQUIRES
        cdc_core
        cdc_ui
        cdc_views
        cdc_os_ui
        cdc_hal
        usb_badge
        serial_cmd
        cdc_log
        mbedtls
        nvs_flash
        freertos
)

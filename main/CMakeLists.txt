idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES cdc_badge cdc_utils nvs_flash freertos esp_timer usb_badge totp fido2 fido2_ble ca serial_cmd wifi ntp
)

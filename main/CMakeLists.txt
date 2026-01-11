idf_component_register(
    SRCS
        "app_fido.cpp"
        "app_globals.cpp"
        "app_input.cpp"
        "app_menus.cpp"
        "app_power.cpp"
        "app_render.cpp"
        "app_state.cpp"
        "app_status.cpp"
        "app_system.cpp"
        "app_totp.cpp"
        "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES cdc_badge cdc_utils nvs_flash freertos esp_timer usb_badge totp fido2 ble_uart ble_badge ca serial_cmd wifi ntp
)

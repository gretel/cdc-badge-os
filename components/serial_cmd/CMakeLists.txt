idf_component_register(
    SRCS "serial_cmd.cpp"
    INCLUDE_DIRS "."
    REQUIRES cdc_utils cdc_badge totp fido2 ca freertos
)

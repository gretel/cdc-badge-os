idf_component_register(
    SRCS "usb_hid.cpp"
    INCLUDE_DIRS "."
    REQUIRES cdc_utils cdc_badge freertos driver espressif__tinyusb
    PRIV_REQUIRES usb
)

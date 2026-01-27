idf_component_register(
    SRCS "ble_core.cpp"
    INCLUDE_DIRS "."
    REQUIRES bt cdc_utils
    PRIV_REQUIRES cdc_badge
)

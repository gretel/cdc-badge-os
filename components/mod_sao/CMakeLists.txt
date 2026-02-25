# SAO Module (Standardized Add-On detection)

idf_component_register(
    SRCS
        "src/SaoModule.cpp"
        "src/sao.cpp"
    INCLUDE_DIRS
        "include"
    REQUIRES
        cdc_core
        cdc_ui
        cdc_views
        cdc_hal
        cdc_log
)

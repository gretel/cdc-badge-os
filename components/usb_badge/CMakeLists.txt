# Read feature flags from header
file(READ "${COMPONENT_DIR}/../cdc_badge/feature_flags.h" FEATURE_FLAGS_CONTENT)

# Check for FEATURE_GPG_CCID
string(REGEX MATCH "#define FEATURE_GPG_CCID ([0-9]+)" FEATURE_GPG_CCID_MATCH "${FEATURE_FLAGS_CONTENT}")
if(FEATURE_GPG_CCID_MATCH)
    set(FEATURE_GPG_CCID ${CMAKE_MATCH_1})
else()
    set(FEATURE_GPG_CCID 0)
endif()

# Base requirements
set(USB_REQUIRES cdc_utils cdc_badge freertos driver espressif__tinyusb)
set(USB_PRIV_REQUIRES usb)

# Add openpgp dependency if CCID is enabled
if(FEATURE_GPG_CCID)
    list(APPEND USB_REQUIRES openpgp)
    message(STATUS "USB Badge: CCID enabled, linking openpgp component")
endif()

idf_component_register(
    SRCS "usb_hid.cpp"
    INCLUDE_DIRS "."
    REQUIRES ${USB_REQUIRES}
    PRIV_REQUIRES ${USB_PRIV_REQUIRES}
)

# Force linker to use our usbd_app_driver_get_cb instead of TinyUSB's weak version
# The --undefined flag ensures our symbol is pulled in before TinyUSB's weak stub
if(FEATURE_GPG_CCID)
    target_link_options(${COMPONENT_LIB} INTERFACE
        "-Wl,--undefined=usbd_app_driver_get_cb"
    )
    message(STATUS "USB Badge: Forcing usbd_app_driver_get_cb linkage for CCID")
endif()

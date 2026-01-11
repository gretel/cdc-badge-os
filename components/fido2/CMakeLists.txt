# Check if FIDO2 is enabled (USB transport only)
set(FEATURE_FIDO2_USB 0)

# Read feature flags from header
file(READ "${CMAKE_CURRENT_LIST_DIR}/../cdc_badge/feature_flags.h" FEATURE_FLAGS_CONTENT)

string(REGEX MATCH "#define FEATURE_FIDO2_USB ([01])" _ ${FEATURE_FLAGS_CONTENT})
if(CMAKE_MATCH_1)
    set(FEATURE_FIDO2_USB ${CMAKE_MATCH_1})
endif()

# FIDO2 enabled if USB transport is enabled
if(FEATURE_FIDO2_USB)
    # Core FIDO2 sources
    set(FIDO2_SRCS
        "cbor_helpers.cpp"
        "ctap2.cpp"
        "fido2.cpp"
        "fido2_storage.cpp"
        "ctaphid.cpp"
        "u2f.cpp"
    )

    idf_component_register(
        SRCS ${FIDO2_SRCS}
        INCLUDE_DIRS "."
        REQUIRES
            cdc_badge
            usb_badge
            mbedtls
            nvs_flash
            freertos
    )

    message(STATUS "FIDO2 component ENABLED (USB=${FEATURE_FIDO2_USB})")
else()
    # Register empty component so dependencies don't break
    idf_component_register(
        INCLUDE_DIRS "."
    )
    message(STATUS "FIDO2 component DISABLED")
endif()

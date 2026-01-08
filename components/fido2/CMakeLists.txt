# Check if FIDO2 is enabled (USB or BT transport)
set(FEATURE_FIDO2_USB 0)
set(FEATURE_FIDO2_BT 0)

# Read feature flags from header
file(READ "${CMAKE_CURRENT_LIST_DIR}/../cdc_badge/feature_flags.h" FEATURE_FLAGS_CONTENT)

string(REGEX MATCH "#define FEATURE_FIDO2_USB ([01])" _ ${FEATURE_FLAGS_CONTENT})
if(CMAKE_MATCH_1)
    set(FEATURE_FIDO2_USB ${CMAKE_MATCH_1})
endif()

string(REGEX MATCH "#define FEATURE_FIDO2_BT ([01])" _ ${FEATURE_FLAGS_CONTENT})
if(CMAKE_MATCH_1)
    set(FEATURE_FIDO2_BT ${CMAKE_MATCH_1})
endif()

# FIDO2 enabled if any transport is enabled
if(FEATURE_FIDO2_USB OR FEATURE_FIDO2_BT)
    # Core FIDO2 sources (always needed)
    set(FIDO2_SRCS
        "cbor_helpers.cpp"
        "ctap2.cpp"
        "fido2.cpp"
        "fido2_storage.cpp"
    )

    # USB transport sources (only if USB enabled)
    if(FEATURE_FIDO2_USB)
        list(APPEND FIDO2_SRCS "ctaphid.cpp" "u2f.cpp")
    endif()

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

    message(STATUS "FIDO2 component ENABLED (USB=${FEATURE_FIDO2_USB}, BT=${FEATURE_FIDO2_BT})")
else()
    # Register empty component so dependencies don't break
    idf_component_register(
        INCLUDE_DIRS "."
    )
    message(STATUS "FIDO2 component DISABLED")
endif()

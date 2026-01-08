# Check if FIDO2_BT is enabled
set(FEATURE_FIDO2_BT 0)  # Default disabled

# Read feature flag from header
file(READ "${CMAKE_CURRENT_LIST_DIR}/../cdc_badge/feature_flags.h" FEATURE_FLAGS_CONTENT)
string(REGEX MATCH "#define FEATURE_FIDO2_BT ([01])" _ ${FEATURE_FLAGS_CONTENT})
if(CMAKE_MATCH_1)
    set(FEATURE_FIDO2_BT ${CMAKE_MATCH_1})
endif()

if(FEATURE_FIDO2_BT)
    idf_component_register(
        SRCS
            "ble_ctap.cpp"
        INCLUDE_DIRS "."
        REQUIRES
            cdc_badge
            fido2
            bt
            nvs_flash
            freertos
    )
else()
    # Register empty component so dependencies don't break
    idf_component_register(
        INCLUDE_DIRS "."
    )
    message(STATUS "FIDO2_BT component DISABLED")
endif()

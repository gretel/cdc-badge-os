# Check if BLE_BADGE is enabled
set(FEATURE_BLE_BADGE 0)  # Default disabled

# Read feature flag from header
file(READ "${CMAKE_CURRENT_LIST_DIR}/../cdc_badge/feature_flags.h" FEATURE_FLAGS_CONTENT)
string(REGEX MATCH "#define FEATURE_BLE_BADGE[ \t]+([01])" _ ${FEATURE_FLAGS_CONTENT})
if(CMAKE_MATCH_1)
    set(FEATURE_BLE_BADGE ${CMAKE_MATCH_1})
endif()

message(STATUS "BLE_BADGE feature flag: ${FEATURE_BLE_BADGE}")

if(FEATURE_BLE_BADGE)
    idf_component_register(
        SRCS
            "ble_badge.cpp"
            "vcard_store.cpp"
        INCLUDE_DIRS "."
        REQUIRES
            cdc_badge
            cdc_utils
            bt
            freertos
            nvs_flash
    )
else()
    # Register empty component so dependencies don't break
    idf_component_register(
        INCLUDE_DIRS "."
    )
    message(STATUS "BLE_BADGE component DISABLED")
endif()

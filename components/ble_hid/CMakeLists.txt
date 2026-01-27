# Check if BLE_HID is enabled
set(FEATURE_BLE_HID 0)  # Default disabled

# Read feature flag from header
file(READ "${CMAKE_CURRENT_LIST_DIR}/../cdc_badge/feature_flags.h" FEATURE_FLAGS_CONTENT)
string(REGEX MATCH "#define FEATURE_BLE_HID[ \t]+([01])" _ ${FEATURE_FLAGS_CONTENT})
if(CMAKE_MATCH_1)
    set(FEATURE_BLE_HID ${CMAKE_MATCH_1})
endif()

message(STATUS "BLE_HID feature flag: ${FEATURE_BLE_HID}")

if(FEATURE_BLE_HID)
    idf_component_register(
        SRCS
            "ble_hid.cpp"
        INCLUDE_DIRS "."
        REQUIRES
            cdc_badge
            cdc_utils
            ble_core
            bt
            freertos
    )
else()
    # Register empty component so dependencies don't break
    idf_component_register(
        INCLUDE_DIRS "."
    )
    message(STATUS "BLE_HID component DISABLED")
endif()

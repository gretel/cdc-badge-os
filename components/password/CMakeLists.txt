# Check if Passwords feature is enabled
set(FEATURE_PASSWORD 0)  # Default disabled

# Read feature flag from header
file(READ "${CMAKE_CURRENT_LIST_DIR}/../cdc_badge/feature_flags.h" FEATURE_FLAGS_CONTENT)
string(REGEX MATCH "#define FEATURE_PASSWORD ([01])" _ ${FEATURE_FLAGS_CONTENT})
if(CMAKE_MATCH_1)
    set(FEATURE_PASSWORD ${CMAKE_MATCH_1})
endif()

if(FEATURE_PASSWORD)
    idf_component_register(
        SRCS "password_store.cpp"
        INCLUDE_DIRS "."
        REQUIRES cdc_utils cdc_badge nvs_flash usb_badge
    )
else()
    idf_component_register(
        INCLUDE_DIRS "."
    )
    message(STATUS "Password store component DISABLED")
endif()

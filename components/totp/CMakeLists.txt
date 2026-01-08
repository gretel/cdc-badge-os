# Check if TOTP is enabled
set(FEATURE_TOTP 0)  # Default disabled

# Read feature flag from header
file(READ "${CMAKE_CURRENT_LIST_DIR}/../cdc_badge/feature_flags.h" FEATURE_FLAGS_CONTENT)
string(REGEX MATCH "#define FEATURE_TOTP ([01])" _ ${FEATURE_FLAGS_CONTENT})
if(CMAKE_MATCH_1)
    set(FEATURE_TOTP ${CMAKE_MATCH_1})
endif()

if(FEATURE_TOTP)
    idf_component_register(
        SRCS "totp.cpp" "totp_store.cpp" "base32.cpp"
        INCLUDE_DIRS "."
        REQUIRES cdc_utils cdc_badge usb_badge freertos mbedtls
    )
else()
    # Register empty component so dependencies don't break
    idf_component_register(
        INCLUDE_DIRS "."
    )
    message(STATUS "TOTP component DISABLED")
endif()

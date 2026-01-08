# Check if CA is enabled
set(FEATURE_CA 0)  # Default disabled

# Read feature flag from header
file(READ "${CMAKE_CURRENT_LIST_DIR}/../cdc_badge/feature_flags.h" FEATURE_FLAGS_CONTENT)
string(REGEX MATCH "#define FEATURE_CA ([01])" _ ${FEATURE_FLAGS_CONTENT})
if(CMAKE_MATCH_1)
    set(FEATURE_CA ${CMAKE_MATCH_1})
endif()

if(FEATURE_CA)
    idf_component_register(
        SRCS
            "ca.cpp"
        INCLUDE_DIRS "."
        REQUIRES
            cdc_badge
            cdc_utils
            nvs_flash
            mbedtls
    )
else()
    # Register empty component so dependencies don't break
    idf_component_register(
        INCLUDE_DIRS "."
    )
    message(STATUS "CA component DISABLED")
endif()

# GPG Component CMakeLists.txt
# Reads FEATURE_GPG from feature_flags.h

# Get the path to feature_flags.h (sibling component)
set(FEATURE_FLAGS_PATH "${COMPONENT_DIR}/../cdc_badge/feature_flags.h")

# Check if file exists, if not try alternative path
if(NOT EXISTS "${FEATURE_FLAGS_PATH}")
    set(FEATURE_FLAGS_PATH "${CMAKE_CURRENT_LIST_DIR}/../cdc_badge/feature_flags.h")
endif()

if(EXISTS "${FEATURE_FLAGS_PATH}")
    file(READ "${FEATURE_FLAGS_PATH}" FEATURE_FLAGS_CONTENT)

    # Check if FEATURE_GPG is enabled (matches "#define FEATURE_GPG 1" with any whitespace)
    string(REGEX MATCH "#define[ \t]+FEATURE_GPG[ \t]+1" FEATURE_GPG_MATCH "${FEATURE_FLAGS_CONTENT}")

    if(FEATURE_GPG_MATCH)
        message(STATUS "GPG: Feature enabled, building gpg component")
        idf_component_register(
            SRCS "gpg.cpp"
            INCLUDE_DIRS "."
            REQUIRES cdc_badge
            PRIV_REQUIRES esp_system nvs_flash mbedtls
        )
    else()
        message(STATUS "GPG: Feature disabled")
        idf_component_register(
            INCLUDE_DIRS "."
        )
    endif()
else()
    message(WARNING "GPG: feature_flags.h not found, disabling GPG component")
    idf_component_register(
        INCLUDE_DIRS "."
    )
endif()

# OpenPGP Component CMakeLists.txt
# Based on pico-openpgp (AGPLv3) - adapted for CDC Badge with TROPIC01
# Reads FEATURE_GPG_CCID from feature_flags.h

# Get the path to feature_flags.h (sibling component)
set(FEATURE_FLAGS_PATH "${COMPONENT_DIR}/../cdc_badge/feature_flags.h")

# Check if file exists, if not try alternative path
if(NOT EXISTS "${FEATURE_FLAGS_PATH}")
    set(FEATURE_FLAGS_PATH "${CMAKE_CURRENT_LIST_DIR}/../cdc_badge/feature_flags.h")
endif()

if(EXISTS "${FEATURE_FLAGS_PATH}")
    file(READ "${FEATURE_FLAGS_PATH}" FEATURE_FLAGS_CONTENT)

    # Check if FEATURE_GPG_CCID is enabled
    string(REGEX MATCH "#define[ \t]+FEATURE_GPG_CCID[ \t]+1" FEATURE_GPG_CCID_MATCH "${FEATURE_FLAGS_CONTENT}")

    if(FEATURE_GPG_CCID_MATCH)
        message(STATUS "OpenPGP: CCID Feature enabled, building openpgp component")
        idf_component_register(
            SRCS
                "openpgp.cpp"
                "ccid.cpp"
                "apdu.cpp"
            INCLUDE_DIRS "."
            REQUIRES cdc_badge gpg
            PRIV_REQUIRES esp_system nvs_flash mbedtls tinyusb
        )
    else()
        message(STATUS "OpenPGP: CCID Feature disabled")
        idf_component_register(
            INCLUDE_DIRS "."
        )
    endif()
else()
    message(WARNING "OpenPGP: feature_flags.h not found, disabling OpenPGP component")
    idf_component_register(
        INCLUDE_DIRS "."
    )
endif()

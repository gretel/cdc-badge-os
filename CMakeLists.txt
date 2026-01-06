cmake_minimum_required(VERSION 3.16.0)

# Patch CalEPD for CDC Badge (MISO support + minimal build)
set(CALEPD_DIR "${CMAKE_SOURCE_DIR}/components/CalEPD")
set(CALEPD_PATCH_MARKER "${CALEPD_DIR}/.patched")

if(NOT EXISTS ${CALEPD_PATCH_MARKER})
    # Overwrite CMakeLists.txt with minimal version (only GDEY029T94)
    file(WRITE "${CALEPD_DIR}/CMakeLists.txt"
"# Minimal CalEPD for CDC Badge (only GDEY029T94)
set(srcs
    \"models/goodisplay/gdey029T94.cpp\"
    \"epd.cpp\"
    \"epdspi.cpp\"
)

idf_component_register(SRCS \${srcs}
    REQUIRES \"Adafruit-GFX\" esp_timer driver
    INCLUDE_DIRS \"include\"
)
")
    message(STATUS "CalEPD: minimal CMakeLists.txt written")

    # Apply MISO patch for TROPIC01 on shared SPI bus
    set(PATCH_MISO "${CMAKE_SOURCE_DIR}/patches/calepd_spi_miso.patch")
    if(EXISTS ${PATCH_MISO})
        execute_process(
            COMMAND patch -p1 -N -d ${CALEPD_DIR} -i ${PATCH_MISO}
            RESULT_VARIABLE PATCH_RESULT
        )
        if(PATCH_RESULT EQUAL 0)
            message(STATUS "CalEPD: MISO patch applied")
        endif()
    endif()

    file(WRITE ${CALEPD_PATCH_MARKER} "Patched for CDC Badge")
endif()

# Exclude upstream or legacy components that are now compiled as part of cdc_badge.
set(EXCLUDE_COMPONENTS libtropic totp fido2 ssh ca pin wifi)
include_directories(BEFORE
    ${CMAKE_SOURCE_DIR}/components/dirent_compat/include
    ${CMAKE_SOURCE_DIR}/include
)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(cdc-badge-nametag)

# Home Assistant Controller Module
#
# Status: prepared but inactive.
# ESP-IDF auto-discovers every directory under components/, so this module
# must register itself even while inactive. The headers-only stub below does
# that without dragging the sources into the build.
#
# To activate the module:
#   1. Comment out the "Inactive registration" line.
#   2. Uncomment the "Active registration" block.
#   3. Add `mod_homeassistant` to the MODULES list in main/CMakeLists.txt.

# --- Inactive registration (default) ---
idf_component_register(INCLUDE_DIRS "include")

# --- Active registration ---
# idf_component_register(
#     SRCS
#         "src/HomeAssistantModule.cpp"
#         "src/HaClient.cpp"
#         "src/HaFavoriteStorage.cpp"
#         "src/HaTokenStorage.cpp"
#         "src/HaHomeView.cpp"
#         "src/HaBrowseView.cpp"
#         "src/HaLightDetailView.cpp"
#         "src/HaWaitingView.cpp"
#         "src/HaSetupWizardView.cpp"
#         "src/HaSerialCommands.cpp"
#     INCLUDE_DIRS
#         "include"
#         "${CMAKE_SOURCE_DIR}/components/serial_cmd/include"
#     REQUIRES
#         cdc_core
#         cdc_ui
#         cdc_views
#         cdc_hal
#         cdc_os_ui
#         cdc_log
#         serial_cmd
#         esp_http_client
#         esp-tls
#         json
#         mbedtls
#         nvs_flash
#         freertos
# )

Import("env")

# Ensure ESP-IDF Component Manager is enabled for esp_tinyusb fetches.
env["ENV"]["IDF_COMPONENT_MANAGER"] = "1"

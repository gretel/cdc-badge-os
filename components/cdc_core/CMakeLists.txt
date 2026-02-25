# CDC Badge OS - Core Component
# ServiceRegistry, EventBus

idf_component_register(
    SRCS
        "src/AttestationKeyService.cpp"
        "src/ServiceRegistry.cpp"
        "src/EventBus.cpp"
        "src/PinManager.cpp"
        "src/ModuleRegistry.cpp"
        "src/TropicStorage.cpp"
        "src/TropicSlotMap.cpp"
        "src/KeyFingerprint.cpp"
        "src/UsbManager.cpp"
        "src/IKeyboardProvider.cpp"
    INCLUDE_DIRS
        "include"
    REQUIRES
        freertos
        esp_timer
        nvs_flash
        cdc_hal
        cdc_log
        mbedtls
)

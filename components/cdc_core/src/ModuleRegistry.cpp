#include "cdc_core/ModuleRegistry.h"
#include "cdc_core/TropicSlotMap.h"
#include "cdc_core/EventBus.h"
#include "cdc_log.h"
#include <nvs_flash.h>
#include <nvs.h>
#include <cstring>
#include <cstdio>
#include <algorithm>

static const char* TAG = "ModuleReg";

namespace cdc::core {

ModuleRegistry& ModuleRegistry::instance() {
    static ModuleRegistry instance;
    return instance;
}

void ModuleRegistry::registerInitializer(ModuleInitFunc initFunc) {
    if (!initFunc) return;

    if (initCount_ >= MAX_INITIALIZERS) {
        LOG_E(TAG, "Module initializer registry full");
        return;
    }

    initializers_[initCount_++] = initFunc;
}

void ModuleRegistry::runAllInitializers() {
    LOG_I(TAG, "Running %d module initializers", initCount_);

    for (uint8_t i = 0; i < initCount_; i++) {
        if (initializers_[i]) {
            initializers_[i]();
        }
    }

    // Load disabled modules list from NVS (name-based, robust against order changes)
    loadDisabledList();

    // After all modules are registered, clean up orphaned NVS data
    cleanupOrphanedModuleData();

    // Save current module list for next boot
    saveModuleList();
}

bool ModuleRegistry::registerModule(IModule* module) {
    if (!module) {
        LOG_E(TAG, "Cannot register null module");
        return false;
    }

    if (count_ >= MAX_MODULES) {
        LOG_E(TAG, "Module registry full, cannot register '%s'", module->getName());
        return false;
    }

    // Check for duplicate
    for (uint8_t i = 0; i < count_; i++) {
        if (strcmp(modules_[i]->getName(), module->getName()) == 0) {
            LOG_W(TAG, "Module '%s' already registered", module->getName());
            return false;
        }
    }

    modules_[count_++] = module;
    clearModuleError(count_ - 1);
    (void)applySlotRequest(module, count_ - 1);
    LOG_I(TAG, "Registered module '%s' v%s", module->getName(), module->getVersion());
    return true;
}

void ModuleRegistry::unregisterModule(const char* name) {
    if (!name) return;

    for (uint8_t i = 0; i < count_; i++) {
        if (strcmp(modules_[i]->getName(), name) == 0) {
            // Shift remaining modules
            for (uint8_t j = i; j < count_ - 1; j++) {
                modules_[j] = modules_[j + 1];
            }
            modules_[--count_] = nullptr;
            LOG_I(TAG, "Unregistered module '%s'", name);
            return;
        }
    }
}

IModule* ModuleRegistry::getModule(const char* name) {
    if (!name) return nullptr;

    for (uint8_t i = 0; i < count_; i++) {
        if (strcmp(modules_[i]->getName(), name) == 0) {
            return modules_[i];
        }
    }
    return nullptr;
}

IModule* ModuleRegistry::getModuleAt(uint8_t index) {
    if (index >= count_) return nullptr;
    return modules_[index];
}

bool ModuleRegistry::initAll() {
    bool allOk = true;
    for (uint8_t i = 0; i < count_; i++) {
        if (!modules_[i]->init()) {
            LOG_E(TAG, "Failed to init module '%s'", modules_[i]->getName());
            allOk = false;
        }
    }
    return allOk;
}

bool ModuleRegistry::startAll() {
    bool allOk = true;
    for (uint8_t i = 0; i < count_; i++) {
        // Skip disabled modules
        if (!isModuleEnabled(i)) {
            LOG_I(TAG, "Module '%s' is disabled, skipping start", modules_[i]->getName());
            continue;
        }

        if (!startModule(i)) {
            allOk = false;
        }
    }
    return allOk;
}

bool ModuleRegistry::startModule(uint8_t index) {
    if (index >= count_) return false;
    IModule* module = modules_[index];
    if (!module) return false;

    if (hasModuleSlotError(index)) {
        LOG_E(TAG, "Module '%s' blocked: %s", module->getName(),
              getModuleSlotError(index) ? getModuleSlotError(index) : "slot map error");
        return false;
    }

    if (module->getState() == ServiceState::INITIALIZED ||
        module->getState() == ServiceState::STOPPED) {
        if (!module->start()) {
            LOG_E(TAG, "Failed to start module '%s'", module->getName());
            return false;
        }
    }

    return true;
}

void ModuleRegistry::stopAll() {
    for (uint8_t i = 0; i < count_; i++) {
        if (modules_[i]->getState() == ServiceState::STARTED) {
            modules_[i]->stop();
        }
    }
}

uint8_t ModuleRegistry::getMenuItems(MenuLocation location, ModuleMenuItem* items, uint8_t maxItems) {
    if (!items || maxItems == 0) return 0;

    uint8_t totalCount = 0;

    // Collect items from all modules
    for (uint8_t i = 0; i < count_; i++) {
        if (modules_[i]->getState() != ServiceState::STARTED) continue;

        ModuleMenuItem moduleItems[8];
        uint8_t count = modules_[i]->getMenuItems(moduleItems, 8);

        for (uint8_t j = 0; j < count && totalCount < maxItems; j++) {
            if (moduleItems[j].location == location) {
                // Check visibility
                if (moduleItems[j].isVisible && !moduleItems[j].isVisible()) {
                    continue;
                }
                // Set module name
                moduleItems[j].moduleName = modules_[i]->getName();
                items[totalCount++] = moduleItems[j];
            }
        }
    }

    // Sort by priority (bubble sort, small array)
    for (uint8_t i = 0; i < totalCount; i++) {
        for (uint8_t j = i + 1; j < totalCount; j++) {
            if (items[j].priority < items[i].priority) {
                ModuleMenuItem tmp = items[i];
                items[i] = items[j];
                items[j] = tmp;
            }
        }
    }

    return totalCount;
}

void ModuleRegistry::dispatchUnlock() {
    for (uint8_t i = 0; i < count_; i++) {
        if (modules_[i]->getState() == ServiceState::STARTED) {
            modules_[i]->onUnlock();
        }
    }
}

void ModuleRegistry::dispatchLock() {
    for (uint8_t i = 0; i < count_; i++) {
        if (modules_[i]->getState() == ServiceState::STARTED) {
            modules_[i]->onLock();
        }
    }
}

void ModuleRegistry::dispatchUsbConnect() {
    for (uint8_t i = 0; i < count_; i++) {
        if (modules_[i]->getState() == ServiceState::STARTED) {
            modules_[i]->onUsbConnect();
        }
    }
}

void ModuleRegistry::dispatchUsbDisconnect() {
    for (uint8_t i = 0; i < count_; i++) {
        if (modules_[i]->getState() == ServiceState::STARTED) {
            modules_[i]->onUsbDisconnect();
        }
    }
}

void ModuleRegistry::dispatchTick(uint32_t nowMs) {
    for (uint8_t i = 0; i < count_; i++) {
        if (modules_[i]->getState() == ServiceState::STARTED) {
            modules_[i]->onTick(nowMs);
        }
    }
}

uint8_t ModuleRegistry::getLockScreenContextItems(LockScreenContextItem* items, uint8_t maxItems) {
    if (!items || maxItems == 0) return 0;

    uint8_t totalCount = 0;

    // Collect items from all modules
    for (uint8_t i = 0; i < count_; i++) {
        if (modules_[i]->getState() != ServiceState::STARTED) continue;

        LockScreenContextItem moduleItems[4];
        uint8_t count = modules_[i]->getLockScreenContextItems(moduleItems, 4);

        for (uint8_t j = 0; j < count && totalCount < maxItems; j++) {
            // Set module name
            moduleItems[j].moduleName = modules_[i]->getName();
            items[totalCount++] = moduleItems[j];
        }
    }

    // Sort by priority (bubble sort, small array)
    for (uint8_t i = 0; i < totalCount; i++) {
        for (uint8_t j = i + 1; j < totalCount; j++) {
            if (items[j].priority < items[i].priority) {
                LockScreenContextItem tmp = items[i];
                items[i] = items[j];
                items[j] = tmp;
            }
        }
    }

    return totalCount;
}

// =============================================================================
// NVS Garbage Collection for removed modules
// =============================================================================

static constexpr const char* MODULES_NVS_NAMESPACE = "modules";
static constexpr const char* MODULES_NVS_KEY = "list";
static constexpr const char* MODULES_NVS_KEY_DISABLED = "disabled";
static constexpr size_t MAX_MODULE_LIST_SIZE = 256;

void ModuleRegistry::cleanupOrphanedModuleData() {
    nvs_handle_t handle;
    if (nvs_open(MODULES_NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        // No previous module list - first boot or NVS was erased
        LOG_I(TAG, "No previous module list found (first boot)");
        return;
    }

    // Read saved module list (comma-separated names)
    char savedList[MAX_MODULE_LIST_SIZE] = {0};
    size_t len = sizeof(savedList);
    esp_err_t err = nvs_get_str(handle, MODULES_NVS_KEY, savedList, &len);
    nvs_close(handle);

    if (err != ESP_OK || len == 0) {
        LOG_I(TAG, "No saved module list");
        return;
    }

    LOG_I(TAG, "Checking for orphaned module data...");

    // Parse comma-separated list and check each module
    char* saveptr = nullptr;
    char* token = strtok_r(savedList, ",", &saveptr);

    while (token) {
        // Skip empty tokens
        if (strlen(token) == 0) {
            token = strtok_r(nullptr, ",", &saveptr);
            continue;
        }

        // Check if this module is currently registered
        bool found = false;
        for (uint8_t i = 0; i < count_; i++) {
            if (strcmp(modules_[i]->getName(), token) == 0) {
                found = true;
                break;
            }
        }

        if (!found) {
            // Module no longer exists - erase its NVS namespace
            char nsName[20];
            snprintf(nsName, sizeof(nsName), "%s%s", NVS_PREFIX, token);

            LOG_W(TAG, "Module '%s' removed - erasing NVS namespace '%s'", token, nsName);

            nvs_handle_t modHandle;
            if (nvs_open(nsName, NVS_READWRITE, &modHandle) == ESP_OK) {
                nvs_erase_all(modHandle);
                nvs_commit(modHandle);
                nvs_close(modHandle);
                LOG_I(TAG, "Erased NVS data for removed module '%s'", token);
            }
        }

        token = strtok_r(nullptr, ",", &saveptr);
    }
}

void ModuleRegistry::saveModuleList() {
    if (count_ == 0) {
        LOG_I(TAG, "No modules to save");
        return;
    }

    // Build comma-separated list of module names
    char moduleList[MAX_MODULE_LIST_SIZE] = {0};
    size_t offset = 0;

    for (uint8_t i = 0; i < count_; i++) {
        const char* name = modules_[i]->getName();
        size_t nameLen = strlen(name);

        // Check if it fits
        if (offset + nameLen + 2 > sizeof(moduleList)) {
            LOG_W(TAG, "Module list too long, truncating");
            break;
        }

        if (offset > 0) {
            moduleList[offset++] = ',';
        }
        memcpy(moduleList + offset, name, nameLen);
        offset += nameLen;
    }
    moduleList[offset] = '\0';

    // Save to NVS
    nvs_handle_t handle;
    if (nvs_open(MODULES_NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_str(handle, MODULES_NVS_KEY, moduleList);
        nvs_commit(handle);
        nvs_close(handle);
        LOG_I(TAG, "Saved module list: %s", moduleList);
    } else {
        LOG_E(TAG, "Failed to save module list");
    }
}

// =============================================================================
// Module Enable/Disable Persistence (Name-based for robustness)
// =============================================================================

void ModuleRegistry::loadDisabledList() {
    nvs_handle_t handle;
    if (nvs_open(MODULES_NVS_NAMESPACE, NVS_READONLY, &handle) == ESP_OK) {
        size_t len = sizeof(disabledModules_);
        if (nvs_get_str(handle, MODULES_NVS_KEY_DISABLED, disabledModules_, &len) == ESP_OK) {
            LOG_I(TAG, "Loaded disabled modules: %s", disabledModules_);
        } else {
            disabledModules_[0] = '\0';
        }
        nvs_close(handle);
    }
}

void ModuleRegistry::saveDisabledList() {
    nvs_handle_t handle;
    if (nvs_open(MODULES_NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_str(handle, MODULES_NVS_KEY_DISABLED, disabledModules_);
        nvs_commit(handle);
        nvs_close(handle);
        LOG_I(TAG, "Saved disabled modules: %s", disabledModules_);
    } else {
        LOG_E(TAG, "Failed to save disabled modules list");
    }
}

bool ModuleRegistry::isModuleEnabledByName(const char* name) const {
    if (!name || name[0] == '\0') return true;
    if (disabledModules_[0] == '\0') return true;  // No disabled modules

    // Search for name in comma-separated list
    size_t nameLen = strlen(name);
    const char* ptr = disabledModules_;

    while (*ptr) {
        // Skip leading commas
        while (*ptr == ',') ptr++;
        if (*ptr == '\0') break;

        // Find end of current token
        const char* end = ptr;
        while (*end && *end != ',') end++;
        size_t tokenLen = end - ptr;

        // Compare
        if (tokenLen == nameLen && strncmp(ptr, name, nameLen) == 0) {
            return false;  // Found in disabled list
        }

        ptr = end;
    }

    return true;  // Not in disabled list = enabled
}

bool ModuleRegistry::isModuleEnabled(uint8_t index) const {
    if (index >= count_) return false;
    return isModuleEnabledByName(modules_[index]->getName());
}

void ModuleRegistry::setModuleEnabled(uint8_t index, bool enabled) {
    if (index >= count_) return;

    const char* name = modules_[index]->getName();
    bool currentlyEnabled = isModuleEnabledByName(name);

    if (enabled == currentlyEnabled) return;  // No change needed

    if (enabled) {
        // Remove name from disabled list
        char newList[MAX_DISABLED_LIST_SIZE] = {0};
        size_t newOffset = 0;
        size_t nameLen = strlen(name);

        const char* ptr = disabledModules_;
        while (*ptr) {
            while (*ptr == ',') ptr++;
            if (*ptr == '\0') break;

            const char* end = ptr;
            while (*end && *end != ',') end++;
            size_t tokenLen = end - ptr;

            // Copy token if it's not the one to remove
            if (!(tokenLen == nameLen && strncmp(ptr, name, nameLen) == 0)) {
                if (newOffset > 0 && newOffset < sizeof(newList) - 1) {
                    newList[newOffset++] = ',';
                }
                if (newOffset + tokenLen < sizeof(newList)) {
                    memcpy(newList + newOffset, ptr, tokenLen);
                    newOffset += tokenLen;
                }
            }

            ptr = end;
        }
        newList[newOffset] = '\0';
        memcpy(disabledModules_, newList, sizeof(disabledModules_));
    } else {
        // Add name to disabled list
        size_t currentLen = strlen(disabledModules_);
        size_t nameLen = strlen(name);

        if (currentLen + nameLen + 2 < MAX_DISABLED_LIST_SIZE) {
            if (currentLen > 0) {
                disabledModules_[currentLen++] = ',';
            }
            memcpy(disabledModules_ + currentLen, name, nameLen + 1);
        } else {
            LOG_W(TAG, "Disabled list full, cannot add '%s'", name);
            return;
        }
    }

    saveDisabledList();
}

bool ModuleRegistry::toggleModuleEnabled(uint8_t index) {
    if (index >= count_) return false;

    bool wasEnabled = isModuleEnabled(index);
    setModuleEnabled(index, !wasEnabled);
    return !wasEnabled;  // Return new state
}

bool ModuleRegistry::hasModuleSlotError(uint8_t index) const {
    if (index >= count_) return false;
    return moduleErrors_[index].hasError;
}

const char* ModuleRegistry::getModuleSlotError(uint8_t index) const {
    if (index >= count_) return nullptr;
    return moduleErrors_[index].hasError ? moduleErrors_[index].message : nullptr;
}

void ModuleRegistry::setModuleError(uint8_t index, const char* message) {
    if (index >= MAX_MODULES) return;
    moduleErrors_[index].hasError = true;
    if (message) {
        strncpy(moduleErrors_[index].message, message, sizeof(moduleErrors_[index].message) - 1);
        moduleErrors_[index].message[sizeof(moduleErrors_[index].message) - 1] = '\0';
    } else {
        moduleErrors_[index].message[0] = '\0';
    }
}

void ModuleRegistry::clearModuleError(uint8_t index) {
    if (index >= MAX_MODULES) return;
    moduleErrors_[index].hasError = false;
    moduleErrors_[index].message[0] = '\0';
}

void ModuleRegistry::reportModuleError(const char* name, const char* message) {
    if (!name) return;

    // Find module by name
    for (uint8_t i = 0; i < count_; i++) {
        if (strcmp(modules_[i]->getName(), name) == 0) {
            // Stop the module if it's running
            if (modules_[i]->getState() == ServiceState::STARTED) {
                modules_[i]->stop();
                LOG_W(TAG, "Module '%s' stopped due to error", name);
            }
            setModuleError(i, message);
            LOG_E(TAG, "Module '%s' error: %s", name, message ? message : "(null)");

            // Publish error event for UI notification
            Event evt;
            evt.type = EventType::MODULE_ERROR;
            evt.data.value = i;
            EventBus::instance().publish(evt);
            return;
        }
    }
    LOG_W(TAG, "reportModuleError: module '%s' not found", name);
}

void ModuleRegistry::clearModuleErrorByName(const char* name) {
    if (!name) return;

    for (uint8_t i = 0; i < count_; i++) {
        if (strcmp(modules_[i]->getName(), name) == 0) {
            clearModuleError(i);
            return;
        }
    }
}

bool ModuleRegistry::retryModule(uint8_t index) {
    if (index >= count_) return false;

    IModule* module = modules_[index];
    if (!module) return false;

    const char* name = module->getName();
    LOG_I(TAG, "Retrying module '%s'...", name);

    // Clear the error first
    clearModuleError(index);

    // Try to re-initialize if needed
    ServiceState state = module->getState();
    if (state == ServiceState::UNINITIALIZED) {
        if (!module->init()) {
            reportModuleError(name, "Init failed on retry");
            return false;
        }
    }

    // Try to start
    if (!module->start()) {
        reportModuleError(name, "Start failed on retry");
        return false;
    }

    LOG_I(TAG, "Module '%s' retry successful", name);
    return true;
}

// =============================================================================
// Slot Validation Helpers
// =============================================================================

void ModuleRegistry::buildSlotErrorMessage(char* buffer, size_t bufSize,
                                           const char* errorType, const char* mapName) {
    snprintf(buffer, bufSize, "%s for %s", errorType, mapName);
}

bool ModuleRegistry::validateSlotMap(const char* moduleName) {
    const auto& slotMap = TropicSlotMap::instance();
    if (!slotMap.isValid()) {
        const char* errMsg = slotMap.errorMessage();
        reportModuleError(moduleName, errMsg ? errMsg : "slot map invalid");
        return false;
    }
    return true;
}

bool ModuleRegistry::validateEccRange(const char* mapName, const char* moduleName,
                                      uint16_t minSlots, IModule::SlotRange& range,
                                      uint8_t& moduleId) {
    const auto& slotMap = TropicSlotMap::instance();
    TropicSlotMap::SlotRange ecc = {};

    if (!slotMap.getRangeByName(mapName, TropicSlotMap::SlotType::ECC, &ecc)) {
        char msg[96];
        buildSlotErrorMessage(msg, sizeof(msg), "missing ECC slot map", mapName);
        reportModuleError(moduleName, msg);
        return false;
    }

    uint16_t count = static_cast<uint16_t>(ecc.end - ecc.start + 1);
    if (count < minSlots) {
        char msg[96];
        buildSlotErrorMessage(msg, sizeof(msg), "not enough ECC slots", mapName);
        reportModuleError(moduleName, msg);
        return false;
    }

    range.hasEcc = true;
    range.eccStart = static_cast<uint8_t>(ecc.start);
    range.eccEnd = static_cast<uint8_t>(ecc.end);
    moduleId = ecc.moduleId;
    return true;
}

bool ModuleRegistry::validateRmemRange(const char* mapName, const char* moduleName,
                                       uint16_t minSlots, IModule::SlotRange& range,
                                       uint8_t& moduleId) {
    const auto& slotMap = TropicSlotMap::instance();
    TropicSlotMap::SlotRange rmem = {};

    if (!slotMap.getRangeByName(mapName, TropicSlotMap::SlotType::RMEM, &rmem)) {
        char msg[96];
        buildSlotErrorMessage(msg, sizeof(msg), "missing RMEM slot map", mapName);
        reportModuleError(moduleName, msg);
        return false;
    }

    uint16_t count = static_cast<uint16_t>(rmem.end - rmem.start + 1);
    if (count < minSlots) {
        char msg[96];
        buildSlotErrorMessage(msg, sizeof(msg), "not enough RMEM slots", mapName);
        reportModuleError(moduleName, msg);
        return false;
    }

    // Check for module ID mismatch (ECC and RMEM must belong to same module)
    if (moduleId != 0 && moduleId != rmem.moduleId) {
        char msg[96];
        buildSlotErrorMessage(msg, sizeof(msg), "module id mismatch", mapName);
        reportModuleError(moduleName, msg);
        return false;
    }

    range.hasRmem = true;
    range.rmemStart = rmem.start;
    range.rmemEnd = rmem.end;
    moduleId = rmem.moduleId;
    return true;
}

// =============================================================================
// Slot Request Application (Orchestrator)
// =============================================================================

bool ModuleRegistry::applySlotRequest(IModule* module, uint8_t index) {
    if (!module) return false;

    const char* moduleName = module->getName();

    // Step 1: Validate slot map is initialized and valid
    if (!validateSlotMap(moduleName)) {
        return false;
    }

    // Step 2: Get slot request from module
    IModule::SlotRequest req = module->getSlotRequest();
    const char* mapName = (req.mapName && req.mapName[0] != '\0') ? req.mapName : moduleName;

    // No slots required - nothing to validate
    if (!mapName || (req.minEccSlots == 0 && req.minRmemSlots == 0)) {
        return true;
    }

    IModule::SlotRange range = {};
    uint8_t moduleId = 0;

    // Step 3: Validate ECC slot range if required
    if (req.minEccSlots > 0) {
        if (!validateEccRange(mapName, moduleName, req.minEccSlots, range, moduleId)) {
            return false;
        }
    }

    // Step 4: Validate RMEM slot range if required
    if (req.minRmemSlots > 0) {
        if (!validateRmemRange(mapName, moduleName, req.minRmemSlots, range, moduleId)) {
            return false;
        }
    }

    // Step 5: Apply validated slot range to module
    range.moduleId = moduleId;
    module->setSlotRange(range);
    return true;
}

} // namespace cdc::core

#include "cdc_core/ServiceRegistry.h"
#include "cdc_log.h"
#include <cstring>

static const char* TAG = "ServiceRegistry";

namespace cdc::core {

ServiceRegistry& ServiceRegistry::instance() {
    static ServiceRegistry instance;
    return instance;
}

bool ServiceRegistry::registerService(const char* name, IService* service) {
    if (!name || !service) {
        LOG_E(TAG, "Invalid parameters");
        return false;
    }

    if (count_ >= MAX_SERVICES) {
        LOG_E(TAG, "Registry full, cannot register '%s'", name);
        return false;
    }

    // Check for duplicate name
    for (size_t i = 0; i < count_; i++) {
        if (strcmp(services_[i].name, name) == 0) {
            LOG_E(TAG, "Service '%s' already registered", name);
            return false;
        }
    }

    services_[count_].name = name;
    services_[count_].service = service;
    count_++;

    LOG_I(TAG, "Registered service '%s'", name);
    return true;
}

IService* ServiceRegistry::getService(const char* name) {
    if (!name) return nullptr;

    for (size_t i = 0; i < count_; i++) {
        if (strcmp(services_[i].name, name) == 0) {
            return services_[i].service;
        }
    }

    return nullptr;
}

bool ServiceRegistry::initAll() {
    LOG_I(TAG, "Initializing %u services...", count_);

    for (size_t i = 0; i < count_; i++) {
        LOG_I(TAG, "  [%u/%u] %s", i + 1, count_, services_[i].name);

        if (!services_[i].service->init()) {
            LOG_E(TAG, "Failed to initialize '%s'", services_[i].name);
            return false;
        }
    }

    LOG_I(TAG, "All services initialized");
    return true;
}

bool ServiceRegistry::startAll() {
    LOG_I(TAG, "Starting %u services...", count_);

    for (size_t i = 0; i < count_; i++) {
        if (services_[i].service->getState() == ServiceState::INITIALIZED ||
            services_[i].service->getState() == ServiceState::STOPPED) {

            if (!services_[i].service->start()) {
                LOG_E(TAG, "Failed to start '%s'", services_[i].name);
                return false;
            }
        }
    }

    LOG_I(TAG, "All services started");
    return true;
}

void ServiceRegistry::stopAll() {
    LOG_I(TAG, "Stopping %u services...", count_);

    // Stop in reverse order
    for (size_t i = count_; i > 0; i--) {
        if (services_[i - 1].service->getState() == ServiceState::STARTED) {
            services_[i - 1].service->stop();
        }
    }

    LOG_I(TAG, "All services stopped");
}

} // namespace cdc::core

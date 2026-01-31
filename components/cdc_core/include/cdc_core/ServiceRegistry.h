#pragma once

#include "IService.h"
#include <cstddef>

namespace cdc::core {

/**
 * Service Locator / Dependency Injection container
 *
 * Manages service registration(Discovery and provides typed access.
 * Uses static allocation - no heap.
 */
class ServiceRegistry {
public:
    static constexpr size_t MAX_SERVICES = 24;

    /**
     * Get singleton instance
     */
    static ServiceRegistry& instance();

    /**
     * Register a service
     * @param name Unique service name (e.g., "display", "keypad")
     * @param service Pointer to service instance (must outlive registry)
     * @return true on success, false if full or duplicate name
     */
    bool registerService(const char* name, IService* service);

    /**
     * Get service by name (untyped)
     * @return nullptr if not found
     */
    IService* getService(const char* name);

    /**
     * Get service by name (typed)
     * Usage: auto* display = registry.get<IDisplay>("display");
     */
    template<typename T>
    T* get(const char* name) {
        return static_cast<T*>(getService(name));
    }

    /**
     * Initialize all registered services
     * @return true if all succeeded
     */
    bool initAll();

    /**
     * Start all registered services
     * @return true if all succeeded
     */
    bool startAll();

    /**
     * Stop all registered services (reverse order)
     */
    void stopAll();

    /**
     * Get number of registered services
     */
    size_t count() const { return count_; }

private:
    ServiceRegistry() = default;
    ServiceRegistry(const ServiceRegistry&) = delete;
    ServiceRegistry& operator=(const ServiceRegistry&) = delete;

    struct Entry {
        const char* name;
        IService* service;
    };

    Entry services_[MAX_SERVICES] = {};
    size_t count_ = 0;
};

// Convenience macro for service access
#define CDC_SERVICE(type, name) \
    cdc::core::ServiceRegistry::instance().get<type>(name)

} // namespace cdc::core

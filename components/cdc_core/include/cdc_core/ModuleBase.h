#pragma once

#include "cdc_core/IModule.h"

namespace cdc::core {

/**
 * \brief Base implementation that handles common module lifecycle boilerplate.
 *
 * Provides default state tracking and the standard `start()` / `stop()` state
 * transitions used by most modules in the system. Concrete modules typically:
 *  - implement `init()` (since initialization is module-specific),
 *  - either inherit `start()` / `stop()` unchanged when no custom logic is
 *    required, or override and delegate to `ModuleBase::start()` /
 *    `ModuleBase::stop()` for the state transition while adding cleanup work.
 *
 * The base class is non-templated and stores `state_` and `name_` so derived
 * classes do not need to repeat these members or boilerplate getters.
 */
class ModuleBase : public IModule {
public:
    /**
     * \brief Constructs a module base with the given module name.
     * \param name Module name used for logging and identification.
     */
    explicit ModuleBase(const char* name) : name_(name) {}

    /**
     * \brief Returns the module name supplied to the constructor.
     * \return Pointer to the module name string.
     */
    const char* getName() const override { return name_; }

    /**
     * \brief Returns the current service state.
     * \return Current `ServiceState` value.
     */
    ServiceState getState() const override { return state_; }

    /**
     * \brief Default `start()` implementation performing the standard transition.
     * \return `true` when the state transition succeeded.
     *
     * Allowed source states are `INITIALIZED` and `STOPPED`. Derived classes
     * with custom start logic should call this base implementation first and
     * propagate its return value, or set `state_` themselves if they require
     * a different transition policy.
     */
    bool start() override {
        if (state_ != ServiceState::INITIALIZED &&
            state_ != ServiceState::STOPPED) {
            return false;
        }
        state_ = ServiceState::STARTED;
        return true;
    }

    /**
     * \brief Default `stop()` implementation that transitions to `STOPPED`.
     */
    void stop() override {
        state_ = ServiceState::STOPPED;
    }

protected:
    const char* name_ = nullptr;
    ServiceState state_ = ServiceState::UNINITIALIZED;
};

} // namespace cdc::core

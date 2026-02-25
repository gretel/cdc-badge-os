#include "cdc_core/IKeyboardProvider.h"
#include "cdc_core/ServiceRegistry.h"

namespace cdc::core {

IKeyboardProvider* getKeyboard() {
    return ServiceRegistry::instance().request<IKeyboardProvider>(ServiceType::KEYBOARD);
}

} // namespace cdc::core

/**
 * \file Raii.h
 * \brief Thin alias layer that re-exports cdc::core RAII wrappers in the
 *        cdc::plugin_manager namespace for source-compatibility.
 *
 * The shared implementations live in cdc_core/Raii.h. New code should prefer
 * the cdc::core:: types directly; this header keeps the plugin-system call
 * sites unchanged.
 */

#pragma once

#include "cdc_core/Raii.h"

namespace cdc::plugin_manager {

using ::cdc::core::CapsFreeDeleter;
using ::cdc::core::CStdFreeDeleter;

template <typename T>
using PsramUniquePtr = ::cdc::core::PsramUniquePtr<T>;

template <typename T>
using CStdUniquePtr = ::cdc::core::CStdUniquePtr<T>;

template <typename T>
inline PsramUniquePtr<T> psramAlloc(std::size_t count) noexcept
{
    return ::cdc::core::psramAlloc<T>(count);
}

}  // namespace cdc::plugin_manager

/**
 * \file SlotTable.h
 * \brief Fixed-capacity, 1-based slot table for host-API resources.
 *
 * Plugins identify host-side resources (HTTP requests, event subscriptions,
 * etc.) by integer handle. The handle is the 1-based slot index; 0 is
 * reserved as "invalid handle". The slot type must expose a `bool used`
 * member - lookup() returns nullptr for both out-of-range ids and unused
 * slots, so callers cannot accidentally observe a stale slot.
 */

#pragma once

#include <array>
#include <cstddef>

namespace cdc::plugin_manager {

template <typename T, std::size_t N>
struct SlotTable {
    std::array<T, N> slots{};

    static constexpr std::size_t capacity() { return N; }

    /// 1-based lookup. Returns nullptr if id is out of range or slot unused.
    T* lookup(int id)
    {
        if (id <= 0 || static_cast<std::size_t>(id) > N) return nullptr;
        T* s = &slots[static_cast<std::size_t>(id) - 1];
        return s->used ? s : nullptr;
    }

    /// First-fit allocate. Returns the slot and writes 1-based id to out_id.
    /// Returns nullptr if the table is full; out_id is set to 0 in that case.
    /// Does NOT mark the slot used - caller initialises the slot then sets
    /// `used = true` once setup succeeds.
    T* allocate(int& out_id)
    {
        for (std::size_t i = 0; i < N; ++i) {
            if (!slots[i].used) {
                out_id = static_cast<int>(i + 1);
                return &slots[i];
            }
        }
        out_id = 0;
        return nullptr;
    }
};

}  // namespace cdc::plugin_manager

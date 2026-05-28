#pragma once

#include "mod_gpg/GpgRecvStore.h"
#include <cstddef>
#include <cstdint>

namespace cdc::mod_gpg {

/**
 * \brief Callback invoked when a remote badge has finished pushing a key.
 *
 * The key has already been written through `GpgRecvStore::addKey()` before
 * this callback fires; consumers use it to refresh UI state.
 */
using XsigReceivedCallback = void (*)(const gpg_recv_key_t& key);

/**
 * \brief Initialise the GPG cross-sign BLE endpoint.
 *
 * Registers the GATT service on the badge so peers can push their public
 * key. Idempotent. Returns `true` if the service is up.
 */
bool ble_gpg_xsig_init();

/**
 * \brief Install / remove the "key received" notification.
 */
void ble_gpg_xsig_set_received_callback(XsigReceivedCallback cb);

/**
 * \brief Push the badge's own public key to a peer.
 *
 * Establishes a connection to `addr`, discovers the GPG cross-sign service,
 * writes the key payload to the RX characteristic and disconnects.
 *
 * The badge's own key is built from `gpg_get_status()`; if no GPG key is
 * configured the call returns `false`.
 */
bool ble_gpg_xsig_send(const uint8_t addr[6], uint8_t addr_type);

} // namespace cdc::mod_gpg

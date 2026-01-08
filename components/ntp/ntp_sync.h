// NTP Sync Component
// Synchronizes time from NTP server

#ifndef NTP_SYNC_H
#define NTP_SYNC_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Types
// ============================================================================

typedef enum {
    NTP_STATE_IDLE,
    NTP_STATE_SYNCING,      // NTP request in progress
    NTP_STATE_SUCCESS,      // Successfully synchronized
    NTP_STATE_FAILED        // Sync failed
} ntp_state_t;

// ============================================================================
// Functions
// ============================================================================

/**
 * Start NTP synchronization.
 * Requires WiFi to be connected.
 * Uses DHCP-provided NTP server if available, else pool.ntp.org.
 * Applies timezone offset from badge_settings.
 *
 * @param dhcp_ntp_server NTP server IP from DHCP (0 to use pool.ntp.org)
 */
void ntp_sync_start(uint32_t dhcp_ntp_server);

/**
 * Get current NTP sync state.
 *
 * @return Current state
 */
ntp_state_t ntp_sync_get_state(void);

/**
 * Stop NTP sync and cleanup.
 * Call after sync completes (success or failure).
 */
void ntp_sync_stop(void);

/**
 * Check if NTP sync timed out.
 * Default timeout: 10 seconds.
 *
 * @return true if timed out
 */
bool ntp_sync_timed_out(void);

#ifdef __cplusplus
}
#endif

#endif // NTP_SYNC_H

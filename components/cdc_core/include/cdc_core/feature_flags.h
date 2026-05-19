/**
 * Feature Flags - Compile-time feature toggles
 *
 * Set to 0 to completely disable a feature (zero overhead).
 * When disabled, related code is not compiled at all.
 */

#pragma once

#include "sdkconfig.h"

// ============================================================================
// Security Features
// ============================================================================

// Secure Serial (require PIN for serial commands)
// Maps from Kconfig CONFIG_SECURE_SERIAL
#ifdef CONFIG_SECURE_SERIAL
#define FEATURE_SECURE_SERIAL 1
#else
#ifndef FEATURE_SECURE_SERIAL
#define FEATURE_SECURE_SERIAL 0
#endif
#endif

// NVS Editor destructive actions (privileged tool)
#ifndef FEATURE_NVS_EDIT
#define FEATURE_NVS_EDIT 0
#endif

// Debug Mode (disables lockouts, useful for development)
#ifndef DEBUG_MODE
#define DEBUG_MODE 1
#endif

// Build profile byte. A mismatch between the byte stored in NVS and the
// byte compiled into the running firmware triggers a complete factory wipe
// (NVS partition + TROPIC01 R-Memory + ECC slots) at the next boot. This
// is the beta-phase software guard; bypass-resistant enforcement against an
// active attacker requires Secure Boot v2 with anti-rollback and is on the
// 1.0 roadmap (see docs/SECURITY.md).
#define BUILD_PROFILE_BYTE \
    ((FEATURE_SECURE_SERIAL ? 0x02 : 0x00) | (DEBUG_MODE ? 0x01 : 0x00))

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

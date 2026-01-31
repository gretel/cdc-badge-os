/**
 * PinManager Implementation
 *
 * PIN storage in TROPIC01 R-Memory Slot 0
 * Combined format for Badge/FIDO2 and OpenPGP PINs
 */

#include "cdc_core/PinManager.h"
#include "cdc_hal/ISecureElement.h"
#include "cdc_log.h"
#include "mbedtls/sha256.h"
#include "esp_random.h"
#include "esp_timer.h"
#include <cstring>

static const char* TAG = "PinManager";

namespace cdc::core {

PinManager& PinManager::instance() {
    static PinManager instance;
    return instance;
}

bool PinManager::init() {
    if (pinLoaded_) return true;

    if (!loadFromStorage()) {
        LOG_I(TAG, "No PINs stored, using defaults");
        loadDefaults();
    }

    pinLoaded_ = true;
    return true;
}

void PinManager::loadDefaults() {
    // Badge/FIDO2 hash
    computeBadgeHash(DEFAULT_BADGE_PIN, badgeHash_);
    badgeRetries_ = MAX_RETRIES;

    // Generate random salts
    generateSalt(pw1Salt_);
    generateSalt(pw3Salt_);

    // Compute KDF hashes with salts
    computeKdfHash(DEFAULT_PW1, pw1Salt_, pw1Hash_);
    computeKdfHash(DEFAULT_PW3, pw3Salt_, pw3Hash_);

    iterations_ = DEFAULT_ITERATIONS;
    pw1Retries_ = MAX_RETRIES;
    pw3Retries_ = MAX_RETRIES;
    badgePinIsSet_ = false;

    LOG_I(TAG, "Loaded default PINs");
}

void PinManager::generateSalt(uint8_t* salt) {
    // Try to get random from SE, fallback to ESP random
    hal::ISecureElement* se = hal::getSecureElementInstance();
    if (se && se->isSessionActive() && se->getRandom(salt, SALT_SIZE)) {
        return;
    }
    // Fallback to ESP32 RNG
    esp_fill_random(salt, SALT_SIZE);
}

bool PinManager::isStorageAvailable() const {
    hal::ISecureElement* se = hal::getSecureElementInstance();
    return se && se->isSessionActive();
}

bool PinManager::loadFromStorage() {
    hal::ISecureElement* se = hal::getSecureElementInstance();
    if (!se || !se->isSessionActive()) {
        LOG_W(TAG, "SE session not active");
        return false;
    }

    uint8_t data[STORAGE_SIZE];
    uint16_t actualLen = 0;

    hal::SeResult result = se->rmemRead(RMEM_SLOT_PIN, data, STORAGE_SIZE, &actualLen);
    if (result != hal::SeResult::OK || actualLen != STORAGE_SIZE || data[0] != MAGIC_V3) {
        LOG_D(TAG, "No valid PIN data (len=%d, magic=0x%02X)", actualLen, data[0]);
        return false;
    }

    size_t pos = 1;

    // Badge hash
    memcpy(badgeHash_, &data[pos], BADGE_HASH_SIZE);
    pos += BADGE_HASH_SIZE;

    // Badge retries
    badgeRetries_ = data[pos++];

    // KDF params (skip algorithm bytes, we know them)
    pos += 2;  // KDF algo + Hash algo

    // Iteration count (big endian)
    iterations_ = (data[pos] << 24) | (data[pos+1] << 16) | (data[pos+2] << 8) | data[pos+3];
    pos += 4;

    // Salts
    memcpy(pw1Salt_, &data[pos], SALT_SIZE);
    pos += SALT_SIZE;
    memcpy(pw3Salt_, &data[pos], SALT_SIZE);
    pos += SALT_SIZE;

    // Hashes
    memcpy(pw1Hash_, &data[pos], KDF_HASH_SIZE);
    pos += KDF_HASH_SIZE;
    memcpy(pw3Hash_, &data[pos], KDF_HASH_SIZE);
    pos += KDF_HASH_SIZE;

    // Retries
    pw1Retries_ = data[pos++];
    pw3Retries_ = data[pos++];

    // Check if badge PIN differs from default
    uint8_t defaultHash[BADGE_HASH_SIZE];
    computeBadgeHash(DEFAULT_BADGE_PIN, defaultHash);
    badgePinIsSet_ = !compareHash(badgeHash_, defaultHash, BADGE_HASH_SIZE);

    LOG_I(TAG, "Loaded PINs from R-Memory (Badge=%d, PW1=%d, PW3=%d retries)",
          badgeRetries_, pw1Retries_, pw3Retries_);
    return true;
}

bool PinManager::saveToStorage() {
    hal::ISecureElement* se = hal::getSecureElementInstance();
    if (!se || !se->isSessionActive()) {
        LOG_E(TAG, "SE session not active");
        return false;
    }

    uint8_t data[STORAGE_SIZE];
    size_t pos = 0;

    data[pos++] = MAGIC_V3;

    // Badge hash
    memcpy(&data[pos], badgeHash_, BADGE_HASH_SIZE);
    pos += BADGE_HASH_SIZE;

    // Badge retries
    data[pos++] = badgeRetries_;

    // KDF params
    data[pos++] = KDF_ITERSALTED_S2K;
    data[pos++] = HASH_SHA256;

    // Iteration count (big endian)
    data[pos++] = (iterations_ >> 24) & 0xFF;
    data[pos++] = (iterations_ >> 16) & 0xFF;
    data[pos++] = (iterations_ >> 8) & 0xFF;
    data[pos++] = iterations_ & 0xFF;

    // Salts
    memcpy(&data[pos], pw1Salt_, SALT_SIZE);
    pos += SALT_SIZE;
    memcpy(&data[pos], pw3Salt_, SALT_SIZE);
    pos += SALT_SIZE;

    // Hashes
    memcpy(&data[pos], pw1Hash_, KDF_HASH_SIZE);
    pos += KDF_HASH_SIZE;
    memcpy(&data[pos], pw3Hash_, KDF_HASH_SIZE);
    pos += KDF_HASH_SIZE;

    // Retries
    data[pos++] = pw1Retries_;
    data[pos++] = pw3Retries_;

    se->rmemErase(RMEM_SLOT_PIN);

    hal::SeResult result = se->rmemWrite(RMEM_SLOT_PIN, data, STORAGE_SIZE);
    if (result != hal::SeResult::OK) {
        LOG_E(TAG, "R-Memory write failed");
        return false;
    }

    LOG_I(TAG, "PINs saved to R-Memory slot %d", RMEM_SLOT_PIN);
    return true;
}

bool PinManager::computeBadgeHash(const char* pin, uint8_t* hashOut) {
    if (!pin || !hashOut) return false;

    uint8_t fullHash[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, (const uint8_t*)pin, strlen(pin));
    mbedtls_sha256_finish(&ctx, fullHash);
    mbedtls_sha256_free(&ctx);

    memcpy(hashOut, fullHash, BADGE_HASH_SIZE);
    return true;
}

bool PinManager::computeKdfHash(const char* pin, const uint8_t* salt, uint8_t* hashOut) {
    if (!pin || !salt || !hashOut) return false;

    // OpenPGP Iterated+Salted S2K (RFC 4880)
    // Hash iteration count bytes of (salt + password) repeated
    size_t pinLen = strlen(pin);
    size_t combined = SALT_SIZE + pinLen;

    // Calculate actual byte count from iteration count
    // OpenPGP uses coded count, here we use direct iteration count
    size_t totalBytes = iterations_;

    uint8_t buffer[64];  // Salt + PIN (max 16)
    memcpy(buffer, salt, SALT_SIZE);
    memcpy(buffer + SALT_SIZE, pin, pinLen);

    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);

    size_t processed = 0;
    while (processed < totalBytes) {
        size_t chunk = (totalBytes - processed < combined) ? (totalBytes - processed) : combined;
        mbedtls_sha256_update(&ctx, buffer, chunk);
        processed += chunk;
    }

    mbedtls_sha256_finish(&ctx, hashOut);
    mbedtls_sha256_free(&ctx);

    return true;
}

bool PinManager::compareHash(const uint8_t* h1, const uint8_t* h2, size_t len) const {
    uint8_t diff = 0;
    for (size_t i = 0; i < len; i++) {
        diff |= h1[i] ^ h2[i];
    }
    return diff == 0;
}

// === Badge/FIDO2 PIN ===

bool PinManager::verifyBadgePin(const char* pin) {
    if (!pin) return false;
    if (!pinLoaded_) init();

    // Check if blocked (retries=0 or lockout active)
    if (isBadgeBlocked()) {
        LOG_W(TAG, "Badge PIN blocked");
        return false;
    }

    uint8_t inputHash[BADGE_HASH_SIZE];
    if (!computeBadgeHash(pin, inputHash)) return false;

    if (compareHash(badgeHash_, inputHash, BADGE_HASH_SIZE)) {
        resetBadgeRetries();
        lockoutActive_ = false;  // Clear lockout on success
        LOG_I(TAG, "Badge PIN verified");
        return true;
    }

    badgeRetries_--;
    saveToStorage();  // Persist retry count
    LOG_W(TAG, "Wrong badge PIN, %d retries left", badgeRetries_);

    // Start lockout timer when retries exhausted
    if (badgeRetries_ == 0) {
        startLockout();
    }
    return false;
}

bool PinManager::changeBadgePin(const char* currentPin, const char* newPin) {
    if (!verifyBadgePin(currentPin)) return false;
    return setBadgePin(newPin);
}

bool PinManager::setBadgePin(const char* newPin) {
    if (!newPin) return false;
    size_t len = strlen(newPin);
    if (len < BADGE_PIN_MIN || len > BADGE_PIN_MAX) {
        LOG_E(TAG, "Badge PIN must be %d-%d digits", BADGE_PIN_MIN, BADGE_PIN_MAX);
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        if (newPin[i] < '0' || newPin[i] > '9') {
            LOG_E(TAG, "PIN must contain only digits");
            return false;
        }
    }

    computeBadgeHash(newPin, badgeHash_);
    badgeRetries_ = MAX_RETRIES;

    uint8_t defaultHash[BADGE_HASH_SIZE];
    computeBadgeHash(DEFAULT_BADGE_PIN, defaultHash);
    badgePinIsSet_ = !compareHash(badgeHash_, defaultHash, BADGE_HASH_SIZE);

    saveToStorage();
    LOG_I(TAG, "Badge PIN changed");
    return true;
}

void PinManager::resetBadgeRetries() {
    if (badgeRetries_ < MAX_RETRIES) {
        badgeRetries_ = MAX_RETRIES;
        saveToStorage();
    }
}

bool PinManager::getBadgePinHash(uint8_t* hashOut) const {
    if (!hashOut) return false;
    memcpy(hashOut, badgeHash_, BADGE_HASH_SIZE);
    return true;
}

bool PinManager::verifyBadgePinHash(const uint8_t* hashIn) const {
    if (!hashIn) return false;
    return compareHash(badgeHash_, hashIn, BADGE_HASH_SIZE);
}

// === OpenPGP PW1 (User PIN) ===

bool PinManager::verifyPW1(const char* pin) {
    if (!pin) return false;
    if (!pinLoaded_) init();
    if (pw1Retries_ == 0) {
        LOG_W(TAG, "PW1 blocked");
        return false;
    }

    uint8_t inputHash[KDF_HASH_SIZE];
    if (!computeKdfHash(pin, pw1Salt_, inputHash)) return false;

    if (compareHash(pw1Hash_, inputHash, KDF_HASH_SIZE)) {
        resetPW1Retries();
        LOG_I(TAG, "PW1 verified");
        return true;
    }

    pw1Retries_--;
    saveToStorage();  // Persist retry count
    LOG_W(TAG, "Wrong PW1, %d retries left", pw1Retries_);
    return false;
}

bool PinManager::changePW1(const char* currentPin, const char* newPin) {
    if (!verifyPW1(currentPin)) return false;
    return setPW1(newPin);
}

bool PinManager::setPW1(const char* newPin) {
    if (!newPin) return false;
    size_t len = strlen(newPin);
    if (len < PW1_MIN || len > PIN_MAX) {
        LOG_E(TAG, "PW1 must be %d-%d digits", PW1_MIN, PIN_MAX);
        return false;
    }

    // Generate new salt
    generateSalt(pw1Salt_);
    computeKdfHash(newPin, pw1Salt_, pw1Hash_);
    pw1Retries_ = MAX_RETRIES;

    saveToStorage();
    LOG_I(TAG, "PW1 changed");
    return true;
}

bool PinManager::getPW1Hash(uint8_t* hashOut) const {
    if (!hashOut) return false;
    memcpy(hashOut, pw1Hash_, KDF_HASH_SIZE);
    return true;
}

bool PinManager::getPW1Salt(uint8_t* saltOut) const {
    if (!saltOut) return false;
    memcpy(saltOut, pw1Salt_, SALT_SIZE);
    return true;
}

void PinManager::resetPW1Retries() {
    if (pw1Retries_ < MAX_RETRIES) {
        pw1Retries_ = MAX_RETRIES;
        saveToStorage();
    }
}

// === OpenPGP PW3 (Admin PIN) ===

bool PinManager::verifyPW3(const char* pin) {
    if (!pin) return false;
    if (!pinLoaded_) init();
    if (pw3Retries_ == 0) {
        LOG_W(TAG, "PW3 blocked");
        return false;
    }

    uint8_t inputHash[KDF_HASH_SIZE];
    if (!computeKdfHash(pin, pw3Salt_, inputHash)) return false;

    if (compareHash(pw3Hash_, inputHash, KDF_HASH_SIZE)) {
        resetPW3Retries();
        LOG_I(TAG, "PW3 verified");
        return true;
    }

    pw3Retries_--;
    saveToStorage();
    LOG_W(TAG, "Wrong PW3, %d retries left", pw3Retries_);
    return false;
}

bool PinManager::changePW3(const char* currentPin, const char* newPin) {
    if (!verifyPW3(currentPin)) return false;
    return setPW3(newPin);
}

bool PinManager::setPW3(const char* newPin) {
    if (!newPin) return false;
    size_t len = strlen(newPin);
    if (len < PW3_MIN || len > PIN_MAX) {
        LOG_E(TAG, "PW3 must be %d-%d digits", PW3_MIN, PIN_MAX);
        return false;
    }

    generateSalt(pw3Salt_);
    computeKdfHash(newPin, pw3Salt_, pw3Hash_);
    pw3Retries_ = MAX_RETRIES;

    saveToStorage();
    LOG_I(TAG, "PW3 changed");
    return true;
}

bool PinManager::getPW3Hash(uint8_t* hashOut) const {
    if (!hashOut) return false;
    memcpy(hashOut, pw3Hash_, KDF_HASH_SIZE);
    return true;
}

bool PinManager::getPW3Salt(uint8_t* saltOut) const {
    if (!saltOut) return false;
    memcpy(saltOut, pw3Salt_, SALT_SIZE);
    return true;
}

void PinManager::resetPW3Retries() {
    if (pw3Retries_ < MAX_RETRIES) {
        pw3Retries_ = MAX_RETRIES;
        saveToStorage();
    }
}

// === Lockout Timer ===

bool PinManager::isBadgeBlocked() const {
    // Blocked if retries exhausted AND lockout still active
    if (badgeRetries_ == 0) {
        return isLockoutActive();
    }
    return false;
}

void PinManager::startLockout() {
    lockoutStartMs_ = esp_timer_get_time() / 1000;  // Convert to ms
    lockoutActive_ = true;
    LOG_W(TAG, "Lockout started for %lu ms", LOCKOUT_DURATION_MS);
}

uint32_t PinManager::getLockoutRemainingMs() const {
    if (!lockoutActive_ || badgeRetries_ > 0) {
        return 0;
    }

    uint32_t nowMs = esp_timer_get_time() / 1000;
    uint32_t elapsed = nowMs - lockoutStartMs_;

    if (elapsed >= LOCKOUT_DURATION_MS) {
        return 0;
    }
    return LOCKOUT_DURATION_MS - elapsed;
}

bool PinManager::isLockoutActive() const {
    if (!lockoutActive_) {
        return false;
    }

    uint32_t remaining = getLockoutRemainingMs();
    if (remaining == 0) {
        // Lockout expired - reset retries (const_cast needed for lazy update)
        const_cast<PinManager*>(this)->lockoutActive_ = false;
        const_cast<PinManager*>(this)->badgeRetries_ = MAX_RETRIES;
        const_cast<PinManager*>(this)->saveToStorage();
        LOG_I(TAG, "Lockout expired, retries reset");
        return false;
    }
    return true;
}

} // namespace cdc::core

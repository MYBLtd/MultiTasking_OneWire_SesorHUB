#include "AuthManager.h"
#include <esp_random.h>

// Static member initialization
std::vector<AuthManager::Session> AuthManager::activeSessions;
SemaphoreHandle_t AuthManager::sessionMutex = nullptr;

// Storage keys
const char* AuthManager::KEY_USERNAME = "auth.username";
const char* AuthManager::KEY_PASSWORD = "auth.password";
const char* AuthManager::KEY_SALT = "auth.salt";

void AuthManager::init() {
    Logger::info("Starting AuthManager initialization");
    
    // Verify stored credentials
    String storedUsername = PreferencesManager::getCredential(KEY_USERNAME);
    String storedSalt = PreferencesManager::getCredential(KEY_SALT);
    String storedHash = PreferencesManager::getCredential(KEY_PASSWORD);
    
    Logger::debug("Stored credentials state:");
    Logger::debug(" - Username: " + (storedUsername.isEmpty() ? "empty" : storedUsername));
    Logger::debug(" - Salt: " + (storedSalt.isEmpty() ? "empty" : storedSalt));
    Logger::debug(" - Hash: " + (storedHash.isEmpty() ? "empty" : storedHash));
    
    // Create mutex if it doesn't exist
    if (!sessionMutex) {
        sessionMutex = xSemaphoreCreateMutex();
        if (!sessionMutex) {
            Logger::error("Failed to create session mutex");
            return;
        }
    }
    
    // Check if credentials exist
    String username = PreferencesManager::getCredential(KEY_USERNAME);
    Logger::info("Current stored username: " + (username.isEmpty() ? "none" : username));
    
    if (username.isEmpty()) {
        Logger::info("No credentials found, setting defaults");
        if (setCredentials("admin", "admin")) {
            Logger::info("Default credentials set successfully");
        } else {
            Logger::error("Failed to set default credentials");
        }
    }
    
    // Clear any existing sessions
    revokeAllSessions();
    Logger::info("AuthManager initialization complete");
}

void AuthManager::reset() {
    Logger::info("Resetting authentication system");
    revokeAllSessions();
    setCredentials("audrey", "tautou");
}

bool AuthManager::setCredentials(const String& username, const String& password) {
    Logger::info("Setting credentials for user: '" + username + "'");

    if (username.length() > MAX_USERNAME_LENGTH || password.length() > MAX_PASSWORD_LENGTH) {
        Logger::error("Username or password exceeds maximum length");
        return false;
    }
    
    // Generate new salt and hash
    String salt = generateSalt();
    String hashedPassword = hashPassword(password, salt);
    
    // Store credentials
    bool success = true;
    success &= PreferencesManager::setCredential(KEY_USERNAME, username.c_str());
    Logger::debug("Username stored: " + String(success));
    
    if (success) {
        success &= PreferencesManager::setCredential(KEY_SALT, salt.c_str());
        Logger::debug("Salt stored: " + String(success));
    }
    
    if (success) {
        success &= PreferencesManager::setCredential(KEY_PASSWORD, hashedPassword.c_str());
        Logger::debug("Password hash stored: " + String(success));
    }
    
    if (success) {
        // Verify storage
        String verifyUsername = PreferencesManager::getCredential(KEY_USERNAME);
        Logger::debug("Verification - Stored username: '" + verifyUsername + "'");
        if (verifyUsername != username) {
            Logger::error("Credential storage verification failed!");
            success = false;
        }
    }
    
    if (success) {
        Logger::info("Credentials successfully updated for user: " + username);
        revokeAllSessions();  // Invalidate all sessions on credential change
    } else {
        Logger::error("Failed to save credentials for user: " + username);
    }
    
    return success;
}

bool AuthManager::validateCredentials(const String& username, const String& password) {
    Logger::info("Validating credentials for user: " + username);
    
    // Get stored credentials
    String storedUsername = PreferencesManager::getCredential(KEY_USERNAME);
    String storedSalt = PreferencesManager::getCredential(KEY_SALT);
    String storedHash = PreferencesManager::getCredential(KEY_PASSWORD);
    
    Logger::debug("Stored username: '" + storedUsername + "'");
    Logger::debug("Input username: '" + username + "'");
    Logger::debug("Input password: '" + password + "'");
    Logger::debug("Stored salt: '" + storedSalt + "'");
    Logger::debug("Stored hash: '" + storedHash + "'");
    
    // Calculate hash for comparison
    String calculatedHash = hashPassword(password, storedSalt);
    Logger::debug("Calculated hash: '" + calculatedHash + "'");
    
    bool valid = (calculatedHash == storedHash && storedUsername == username);
    Logger::info("Auth result: " + String(valid ? "Success" : "Failure"));
    
    return valid;
}

String AuthManager::createSession(const String& username) {
    cleanExpiredSessions();
    
    String token = generateToken();
    Session newSession = {
        .token = token,
        .username = username,
        .expiry = (millis() / 1000) + SESSION_LIFETIME
    };
    
    if (xSemaphoreTake(sessionMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        activeSessions.push_back(newSession);
        xSemaphoreGive(sessionMutex);
        Logger::info("Created new session for user: " + username);
    }
    
    return token;
}

bool AuthManager::validateSession(const String& token) {
    if (token.length() != SESSION_TOKEN_LENGTH) {
        return false;
    }
    
    cleanExpiredSessions();
    
    bool valid = false;
    if (xSemaphoreTake(sessionMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        auto it = std::find_if(activeSessions.begin(), activeSessions.end(),
            [&token](const Session& session) {
                return session.token == token && !session.isExpired();
            });
        valid = (it != activeSessions.end());
        xSemaphoreGive(sessionMutex);
    }
    
    return valid;
}

void AuthManager::revokeSession(const String& token) {
    if (xSemaphoreTake(sessionMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        auto it = std::remove_if(activeSessions.begin(), activeSessions.end(),
            [&token](const Session& session) {
                return session.token == token;
            });
        activeSessions.erase(it, activeSessions.end());
        xSemaphoreGive(sessionMutex);
    }
}

void AuthManager::revokeAllSessions() {
    if (xSemaphoreTake(sessionMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        activeSessions.clear();
        xSemaphoreGive(sessionMutex);
        Logger::info("All sessions revoked");
    }
}

String AuthManager::hashPassword(const String& password, const String& salt) {
    String combined = password + salt;
    uint8_t hash[32];
    
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
    mbedtls_md_starts(&ctx);
    mbedtls_md_update(&ctx, (const uint8_t*)combined.c_str(), combined.length());
    mbedtls_md_finish(&ctx, hash);
    mbedtls_md_free(&ctx);
    
    char hashStr[65];
    for(int i = 0; i < 32; i++) {
        sprintf(hashStr + (i * 2), "%02x", hash[i]);
    }
    hashStr[64] = 0;
    
    return String(hashStr);
}

String AuthManager::generateSalt() {
    return generateToken().substring(0, 16);
}

String AuthManager::generateToken() {
    const char charset[] = "0123456789"
                          "abcdefghijklmnopqrstuvwxyz"
                          "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    String token;
    token.reserve(SESSION_TOKEN_LENGTH);
    
    for (size_t i = 0; i < SESSION_TOKEN_LENGTH; i++) {
        uint32_t r = esp_random();
        token += charset[r % (sizeof(charset) - 1)];
    }
    
    return token;
}

void AuthManager::cleanExpiredSessions() {
    if (xSemaphoreTake(sessionMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        auto it = std::remove_if(activeSessions.begin(), activeSessions.end(),
            [](const Session& session) {
                return session.isExpired();
            });
        if (it != activeSessions.end()) {
            size_t removed = std::distance(it, activeSessions.end());
            activeSessions.erase(it, activeSessions.end());
            Logger::debug("Removed " + String(removed) + " expired sessions");
        }
        xSemaphoreGive(sessionMutex);
    }
}

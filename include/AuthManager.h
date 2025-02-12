#pragma once

#include <Arduino.h>
#include <vector>
#include <algorithm>
#include <mbedtls/md.h>
#include "PreferencesManager.h"
#include "Logger.h"

class AuthManager {
public:
    // Core functionality
    static void init();
    static void reset();
    
    // Credential management
    static bool setCredentials(const String& username, const String& password);
    static bool validateCredentials(const String& username, const String& password);
    
    // Session management
    static String createSession(const String& username);
    static bool validateSession(const String& token);
    static void revokeSession(const String& token);
    static void revokeAllSessions();
    
    // Add these public methods
    static String getStoredUsername() {
        return PreferencesManager::getCredential(KEY_USERNAME);
    }
    
    static String getStoredSalt() {
        return PreferencesManager::getCredential(KEY_SALT);
    }
    
    static String getStoredHash() {
        return PreferencesManager::getCredential(KEY_PASSWORD);
    }
    
    // Constants
    static const size_t MAX_USERNAME_LENGTH = 32;
    static const size_t MAX_PASSWORD_LENGTH = 64;
    static const size_t SESSION_TOKEN_LENGTH = 32;
    static const uint32_t SESSION_LIFETIME = 24 * 60 * 60;  // 24 hours in seconds

private:
    struct Session {
        String token;
        String username;
        uint32_t expiry;
        
        bool isExpired() const {
            return (millis() / 1000) > expiry;
        }
    };
    
    // Helper methods
    static String hashPassword(const String& password, const String& salt);
    static String generateSalt();
    static String generateToken();
    static void cleanExpiredSessions();
    
    // Storage keys
    static const char* KEY_USERNAME;
    static const char* KEY_PASSWORD;
    static const char* KEY_SALT;
    
    // Session management
    static std::vector<Session> activeSessions;
    static SemaphoreHandle_t sessionMutex;
    
    // No instantiation
    AuthManager() = delete;
};

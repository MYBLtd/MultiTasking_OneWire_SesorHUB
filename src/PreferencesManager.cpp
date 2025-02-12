#include "PreferencesManager.h"
#include "ESP32PreferenceStorage.h"

// Static member initialization
PreferenceStorage* PreferencesManager::prefs = nullptr;
SemaphoreHandle_t PreferencesManager::prefsMutex = nullptr;

void PreferencesManager::init() {
    Logger::info("Initializing PreferencesManager");

    // Create mutex if it doesn't exist
    if (!prefsMutex) {
        prefsMutex = xSemaphoreCreateMutex();
        if (!prefsMutex) {
            Logger::error("Failed to create preferences mutex");
            return;
        }
        Logger::debug("Created preferences mutex");
    }

    // Create preferences storage if it doesn't exist
    if (!prefs) {
        prefs = new ESP32PreferenceStorage();
        if (!prefs) {
            Logger::error("Failed to create preferences storage");
            return;
        }
        Logger::debug("Created preferences storage");
    }

    // Initialize storage
    if (!prefs->begin("tempmon", false)) {
        Logger::error("Failed to begin preferences storage");
        return;
    }

    if (acquireMutex("init")) {
        // Check if this is first run
        if (prefs->getString("initialized", "").length() == 0) {
            Logger::info("First run detected - initializing preferences");
            
            // Set initialization flag
            prefs->putString("initialized", "true");
            
            // Set default configurations
            prefs->putUInt("auto_scan", 1);  // Enable auto-scan by default
            prefs->putUInt("scan_interval", DEFAULT_SCAN_INTERVAL);
            prefs->putUInt("display_bright", 7);  // Medium brightness
            prefs->putUInt("display_timeout", 30);  // 30 second timeout
            
            Logger::info("Default configurations set");
        }
        
        releaseMutex();
        Logger::info("PreferencesManager initialization complete");
    }
}

void PreferencesManager::reset() {
    Logger::info("Resetting preferences to defaults");
    
    if (acquireMutex("reset")) {
        // Remove all keys one by one instead of using clear
        removeCredential("auth.username");
        removeCredential("auth.password");
        removeCredential("auth.salt");
        removeCredential("initialized");
        
        // Reinitialize with defaults
        init();
        
        releaseMutex();
        Logger::info("Preferences reset complete");
    }
}

// Credential Management Methods
bool PreferencesManager::setCredential(const char* key, const char* value) {
    if (!isInitialized() || !key || !value) {
        Logger::error("Invalid parameters in setCredential");
        return false;
    }

    bool success = false;
    if (acquireMutex("setCredential")) {
        success = prefs->putString(key, value);
        if (success) {
            Logger::debug("Successfully stored credential: " + String(key));
        } else {
            Logger::error("Failed to store credential: " + String(key));
        }
        releaseMutex();
    }
    return success;
}

String PreferencesManager::getCredential(const char* key) {
    if (!isInitialized() || !key) {
        Logger::error("Invalid parameters in getCredential");
        return "";
    }

    String value;
    if (acquireMutex("getCredential")) {
        value = prefs->getString(key, "");
        Logger::debug("Retrieved credential for key: " + String(key) + 
                     ", exists: " + String(!value.isEmpty()));
        releaseMutex();
    }
    return value;
}

bool PreferencesManager::hasCredential(const char* key) {
    return !getCredential(key).isEmpty();
}

bool PreferencesManager::removeCredential(const char* key) {
    if (!isInitialized() || !key) {
        Logger::error("Invalid parameters in removeCredential");
        return false;
    }

    bool success = false;
    if (acquireMutex("removeCredential")) {
        success = prefs->remove(key);
        Logger::debug("Removed credential: " + String(key) + 
                     ", success: " + String(success));
        releaseMutex();
    }
    return success;
}

// MQTT Configuration Methods
// In PreferencesManager.cpp, modify the MQTT config methods:

bool PreferencesManager::setMqttConfig(const char* server, uint16_t port, 
    const char* username, const char* password) {
if (!isInitialized()) return false;

bool success = false;
if (acquireMutex("setMqttConfig")) {
// Remove any existing broker config first
prefs->remove("mqtt.broker");

// Store the new broker address directly
success = prefs->putString("mqtt.broker", server);
Logger::debug("Setting MQTT broker to: " + String(server));

if (success) {
success &= prefs->putUInt("mqtt.port", port);
success &= prefs->putString("mqtt.username", username);

if (password && strlen(password) > 0) {
success &= prefs->putString("mqtt.password", password);
}
}

// Verify the stored value
String storedBroker = prefs->getString("mqtt.broker", "");
Logger::debug("Verified stored broker: " + storedBroker);

releaseMutex();
Logger::info("MQTT configuration " + String(success ? "saved" : "failed"));
}
return success;
}

void PreferencesManager::getMqttConfig(char* server, unsigned short& port, 
    char* username, char* password) {
if (!isInitialized()) return;

if (acquireMutex("getMqttConfig")) {
// Get broker address directly
String broker = prefs->getString("mqtt.broker", "");
Logger::debug("Retrieved MQTT broker: " + broker);

// Copy broker address safely
strncpy(server, broker.c_str(), MAX_MQTT_SERVER_LENGTH - 1);
server[MAX_MQTT_SERVER_LENGTH - 1] = '\0';

port = (unsigned short)prefs->getUInt("mqtt.port", 0);

String user = prefs->getString("mqtt.username", "");
strncpy(username, user.c_str(), MAX_MQTT_CRED_LENGTH - 1);
username[MAX_MQTT_CRED_LENGTH - 1] = '\0';

String pass = prefs->getString("mqtt.password", "");
strncpy(password, pass.c_str(), MAX_MQTT_CRED_LENGTH - 1);
password[MAX_MQTT_CRED_LENGTH - 1] = '\0';

releaseMutex();
}
}

bool PreferencesManager::isMqttConfigured() {
    if (!isInitialized()) return false;
    
    bool configured = false;
    if (acquireMutex("isMqttConfigured")) {
        String broker = prefs->getString("mqtt.broker", "");
        uint32_t port = prefs->getUInt("mqtt.port", 0);
        configured = (broker.length() > 0 && port > 0);
        releaseMutex();
    }
    return configured;
}

bool PreferencesManager::clearMqttConfig() {
    if (!isInitialized()) return false;
    
    bool success = false;
    if (acquireMutex("clearMqttConfig")) {
        success = true;
        success &= prefs->remove("mqtt.broker");
        success &= prefs->remove("mqtt.port");
        success &= prefs->remove("mqtt.username");
        success &= prefs->remove("mqtt.password");
        releaseMutex();
    }
    return success;
}

// Sensor Management Methods
bool PreferencesManager::setSensorName(const uint8_t* address, const char* name) {
    if (!isInitialized() || !address || !name) return false;
    
    bool success = false;
    if (acquireMutex("setSensorName")) {
        String key = getSensorKey(address);
        success = prefs->putString(key.c_str(), name);
        if (success) {
            Logger::info("Saved name '" + String(name) + "' for sensor " + 
                        addressToString(address));
        }
        releaseMutex();
    }
    return success;
}

String PreferencesManager::getSensorName(const uint8_t* address) {
    if (!isInitialized() || !address) return "";
    
    String name;
    if (acquireMutex("getSensorName")) {
        String key = getSensorKey(address);
        name = prefs->getString(key.c_str(), "");
        releaseMutex();
    }
    return name;
}

// Utility Methods
bool PreferencesManager::acquireMutex(const char* caller) {
    if (!prefsMutex) {
        Logger::error("Mutex not initialized in " + String(caller));
        return false;
    }
    
    if (xSemaphoreTake(prefsMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        Logger::error("Failed to acquire mutex in " + String(caller));
        return false;
    }
    return true;
}

void PreferencesManager::releaseMutex() {
    if (prefsMutex) {
        xSemaphoreGive(prefsMutex);
    }
}

bool PreferencesManager::isInitialized() {
    if (!prefs || !prefsMutex) {
        Logger::error("PreferencesManager not initialized");
        return false;
    }
    return true;
}

String PreferencesManager::getSensorKey(const uint8_t* address) {
    char key[15];
    snprintf(key, sizeof(key), "s_%02X%02X%02X%02X",
             address[4], address[5], address[6], address[7]);
    return String(key);
}

String PreferencesManager::addressToString(const uint8_t* address) {
    char temp[17];
    snprintf(temp, sizeof(temp), "%02X%02X%02X%02X%02X%02X%02X%02X",
             address[0], address[1], address[2], address[3],
             address[4], address[5], address[6], address[7]);
    return String(temp);
}

void PreferencesManager::stringToAddress(const String& str, uint8_t* address) {
    Logger::debug("Converting string to address: " + str);
    for (int i = 0; i < 8; i++) {
        if (str.length() >= (i + 1) * 2) {
            String byteStr = str.substring(i * 2, (i + 1) * 2);
            address[i] = strtol(byteStr.c_str(), nullptr, 16);
        } else {
            address[i] = 0;
        }
    }
}

void PreferencesManager::setAutoScanEnabled(bool enabled) {
    if (!isInitialized()) return;
    
    if (acquireMutex("setAutoScanEnabled")) {
        prefs->putUInt("auto_scan", enabled ? 1 : 0);
        releaseMutex();
    }
}

bool PreferencesManager::getAutoScanEnabled() {
    if (!isInitialized()) return true;  // Default to enabled
    
    bool enabled = true;
    if (acquireMutex("getAutoScanEnabled")) {
        enabled = prefs->getUInt("auto_scan", 1) != 0;
        releaseMutex();
    }
    return enabled;
}

void PreferencesManager::setScanInterval(uint32_t seconds) {
    if (!isInitialized()) return;
    
    if (acquireMutex("setScanInterval")) {
        prefs->putUInt("scan_interval", seconds);
        releaseMutex();
    }
}

uint32_t PreferencesManager::getScanInterval() {
    if (!isInitialized()) return DEFAULT_SCAN_INTERVAL;
    
    uint32_t interval = DEFAULT_SCAN_INTERVAL;
    if (acquireMutex("getScanInterval")) {
        interval = prefs->getUInt("scan_interval", DEFAULT_SCAN_INTERVAL);
        releaseMutex();
    }
    return interval;
}

void PreferencesManager::getDisplaySensor(uint8_t* address) {
    if (!isInitialized()) {
        memset(address, 0, 8);
        return;
    }
    
    if (acquireMutex("getDisplaySensor")) {
        String addrStr = prefs->getString("display_sensor", "0000000000000000");
        stringToAddress(addrStr, address);
        releaseMutex();
    }
}

bool PreferencesManager::setDisplaySensor(const uint8_t* address) {
    if (!isInitialized()) return false;
    
    bool success = false;
    if (acquireMutex("setDisplaySensor")) {
        String addrStr = addressToString(address);
        success = prefs->putString("display_sensor", addrStr.c_str());
        releaseMutex();
    }
    return success;
}

bool PreferencesManager::setRelayName(uint8_t relayId, const char* name) {
    if (!isInitialized() || relayId > 1) return false;
    
    bool success = false;
    if (acquireMutex("setRelayName")) {
        String key = "relay_" + String(relayId);
        success = prefs->putString(key.c_str(), name);
        releaseMutex();
    }
    return success;
}

String PreferencesManager::getRelayName(uint8_t relayId) {
    if (!isInitialized() || relayId > 1) return "";
    
    String name;
    if (acquireMutex("getRelayName")) {
        String key = "relay_" + String(relayId);
        name = prefs->getString(key.c_str(), "");
        releaseMutex();
    }
    return name;
}
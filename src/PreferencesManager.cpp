// PreferencesManager.cpp
#include "PreferencesManager.h"
#include "ESP32PreferenceStorage.h"
#include "Logger.h"
#include "Config.h"

// Initialize static members - this ensures we start with null pointers
PreferenceStorage* PreferencesManager::prefs = nullptr;
SemaphoreHandle_t PreferencesManager::prefsMutex = nullptr;

void PreferencesManager::init() {
    prefsMutex = xSemaphoreCreateMutex();
    
    if (!prefs) {
        prefs = new ESP32PreferenceStorage();
    }
    
    if (prefs && prefsMutex) {
        if (prefs->begin("tempmon", false)) {
            if (xSemaphoreTake(prefsMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                // Check if this is first run
                if (prefs->getString("initialized", "").length() == 0) {
                    Logger::info("First run detected - initializing preferences");
                    
                    // Set initialization flag
                    prefs->putString("initialized", "true");
                    
                    // Set up default auto-scan configuration
                    prefs->putUInt("auto_scan", 1);  // Enable by default
                    Logger::info("Set default auto-scan configuration");
                    
                    // Set up default display configuration
                    prefs->putUInt("display_bright", 7);  // Medium brightness
                    prefs->putUInt("display_timeout", 30);  // 30 second timeout
                    Logger::info("Set default display configuration");
                }
                
                xSemaphoreGive(prefsMutex);
            }
        } else {
            Logger::error("Failed to begin preferences");
        }
    } else {
        Logger::error("Failed to initialize preferences system");
    }
}

void PreferencesManager::printCurrentPreferences() {
    if (!prefsMutex || !prefs) return;
    
    if (xSemaphoreTake(prefsMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        Logger::info("Current Preferences Configuration:");
        Logger::info("--------------------------------");
        
        // Print MQTT Configuration
        String stored = prefs->getString("mqtt.broker", "");
        String broker;
        
        // Parse the length-prefixed broker name
        int colonPos = stored.indexOf(':');
        if (colonPos > 0) {
            String lengthStr = stored.substring(0, colonPos);
            int expectedLength = lengthStr.toInt();
            broker = stored.substring(colonPos + 1);
            
            // Verify length matches
            if (broker.length() != expectedLength) {
                broker = "Invalid configuration";
                Logger::warning("Broker length mismatch in printCurrentPreferences");
            }
        } else {
            broker = "Not configured";
        }
        
        uint32_t port = prefs->getUInt("mqtt.port", 0);
        String username = prefs->getString("mqtt.username", "");
        
        Logger::info("MQTT Configuration:");
        Logger::info("  Broker: " + broker);
        Logger::info("  Port: " + String(port));
        Logger::info("  Username: " + username);
        Logger::info("  Password: ********");
        
        // Rest of the configuration printing...
        
        xSemaphoreGive(prefsMutex);
    }
}

// MQTT Configuration Methods
bool PreferencesManager::setMqttConfig(const char* server, uint16_t port, 
                                     const char* username, const char* password) {
    if (!prefsMutex || !prefs) return false;
    
    bool success = false;
    if (xSemaphoreTake(prefsMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        // Create a length-prefixed string
        String lengthPrefixedBroker = String(strlen(server)) + ":" + String(server);
        Logger::debug("Storing MQTT broker with prefix: " + lengthPrefixedBroker);
        
        success = true;
        success &= prefs->putString("mqtt.broker", lengthPrefixedBroker.c_str());
        success &= prefs->putUInt("mqtt.port", port);
        success &= prefs->putString("mqtt.username", username);
        
        if (strlen(password) > 0) {
            success &= prefs->putString("mqtt.password", password);
        }
        
        // Verify storage
        String stored = prefs->getString("mqtt.broker", "");
        Logger::debug("Verification - Stored value: " + stored);
        
        xSemaphoreGive(prefsMutex);
    }
    return success;
}

void PreferencesManager::getMqttConfig(char* server, unsigned short& port, 
                                     char* username, char* password) {
    if (!prefsMutex || !prefs) return;
    
    if (xSemaphoreTake(prefsMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        // Get the length-prefixed string
        String stored = prefs->getString("mqtt.broker", "");
        Logger::debug("Retrieved stored broker: " + stored);
        
        // Parse the length prefix
        int colonPos = stored.indexOf(':');
        if (colonPos > 0) {
            String lengthStr = stored.substring(0, colonPos);
            int expectedLength = lengthStr.toInt();
            String brokerName = stored.substring(colonPos + 1);
            
            // Verify the length matches
            if (brokerName.length() == expectedLength) {
                strncpy(server, brokerName.c_str(), MAX_MQTT_SERVER_LENGTH - 1);
                server[MAX_MQTT_SERVER_LENGTH - 1] = '\0';
                Logger::debug("Successfully parsed broker: " + String(server));
            } else {
                Logger::error("Length mismatch in stored broker name");
                server[0] = '\0';
            }
        } else {
            Logger::error("Invalid broker storage format");
            server[0] = '\0';
        }
        
        // Get other values as normal
        port = (unsigned short)prefs->getUInt("mqtt.port", 0);
        
        String user = prefs->getString("mqtt.username", "");
        strncpy(username, user.c_str(), MAX_MQTT_CRED_LENGTH - 1);
        username[MAX_MQTT_CRED_LENGTH - 1] = '\0';
        
        String pass = prefs->getString("mqtt.password", "");
        strncpy(password, pass.c_str(), MAX_MQTT_CRED_LENGTH - 1);
        password[MAX_MQTT_CRED_LENGTH - 1] = '\0';
        
        xSemaphoreGive(prefsMutex);
    }
}

bool PreferencesManager::isMqttConfigured() {
    if (!prefsMutex || !prefs) return false;
    
    bool isConfigured = false;
    if (xSemaphoreTake(prefsMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        String broker = prefs->getString("mqtt.broker", "");
        uint32_t port = prefs->getUInt("mqtt.port", 0);
        
        isConfigured = (broker.length() > 0 && broker.length() < MAX_MQTT_SERVER_LENGTH &&
                       port > 0 && port <= 65535);
                       
        xSemaphoreGive(prefsMutex);
    }
    return isConfigured;
}

bool PreferencesManager::clearMqttConfig() {
    if (!prefsMutex || !prefs) return false;
    
    bool success = false;
    if (xSemaphoreTake(prefsMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        success = true;
        success &= prefs->remove("mqtt.broker");
        success &= prefs->remove("mqtt.port");
        success &= prefs->remove("mqtt.username");
        success &= prefs->remove("mqtt.password");
        xSemaphoreGive(prefsMutex);
    }
    return success;
}
// Auto-scan Configuration Methods
void PreferencesManager::setAutoScanEnabled(bool enabled) {
    if (!prefsMutex || !prefs) return;
    
    if (xSemaphoreTake(prefsMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        prefs->putUInt("auto_scan", enabled ? 1 : 0);
        xSemaphoreGive(prefsMutex);
    }
}

bool PreferencesManager::getAutoScanEnabled() {
    if (!prefsMutex || !prefs) return true;  // Default to enabled
    
    bool enabled = true;
    if (xSemaphoreTake(prefsMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        enabled = prefs->getUInt("auto_scan", 1) != 0;
        xSemaphoreGive(prefsMutex);
    }
    return enabled;
}

uint32_t PreferencesManager::getScanInterval() {
    if (!prefsMutex || !prefs) return DEFAULT_SCAN_INTERVAL;
    
    uint32_t interval = DEFAULT_SCAN_INTERVAL;
    if (xSemaphoreTake(prefsMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        interval = prefs->getUInt("scan_interval", DEFAULT_SCAN_INTERVAL);
        xSemaphoreGive(prefsMutex);
    }
    return interval;
}

void PreferencesManager::setScanInterval(uint32_t seconds) {
    if (!prefsMutex || !prefs) return;
    
    if (xSemaphoreTake(prefsMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        prefs->putUInt("scan_interval", seconds);
        Logger::debug("Scan interval set to " + String(seconds) + "s");
        xSemaphoreGive(prefsMutex);
    }
}

// Helper method for generating shortened sensor keys
String PreferencesManager::getSensorKey(const uint8_t* address) {
    char key[15];  // Maximum key length for NVS
    snprintf(key, sizeof(key), "s_%02X%02X%02X%02X",
             address[4], address[5], address[6], address[7]);
    return String(key);
}

// Sensor Management Methods
bool PreferencesManager::setSensorName(const uint8_t* address, const char* name) {
    if (!prefsMutex || !prefs) return false;
    
    bool success = false;
    if (xSemaphoreTake(prefsMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        String key = getSensorKey(address);
        success = prefs->putString(key.c_str(), name);
        
        if (success) {
            Logger::info("Successfully saved name '" + String(name) + "' for sensor " + 
                        addressToString(address) + " with key " + key);
        } else {
            Logger::error("Failed to save name for sensor: " + addressToString(address));
        }
        
        xSemaphoreGive(prefsMutex);
    }
    return success;
}

String PreferencesManager::getSensorName(const uint8_t* address) {
    if (!prefsMutex || !prefs || !address) return "";
    
    String name;
    bool mutexAcquired = false;
    
    try {
        mutexAcquired = (xSemaphoreTake(prefsMutex, pdMS_TO_TICKS(100)) == pdTRUE);
        if (!mutexAcquired) {
            Logger::error("Failed to acquire mutex in getSensorName");
            return "";
        }
        
        String key = getSensorKey(address);
        name = prefs->getString(key.c_str(), "");
        
        xSemaphoreGive(prefsMutex);
    } catch (...) {
        if (mutexAcquired) {
            xSemaphoreGive(prefsMutex);
        }
        Logger::error("Exception in getSensorName");
    }
    
    return name;
}

bool PreferencesManager::setDisplaySensor(const uint8_t* address) {
    if (!prefsMutex || !prefs) {
        Logger::error("Preferences not initialized in setDisplaySensor");
        return false;
    }
    
    if (xSemaphoreTake(prefsMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        String addrStr = addressToString(address);
        Logger::debug("Setting display sensor to: " + addrStr);
        
        // Store in preferences
        bool success = prefs->putString("display_sensor", addrStr.c_str());
        if (success) {
            Logger::debug("Successfully stored display sensor preference");
        } else {
            Logger::error("Failed to store display sensor preference");
        }
        
        xSemaphoreGive(prefsMutex);
        return success;
    }
    
    Logger::error("Failed to acquire mutex in setDisplaySensor");
    return false;
}

// Utility Methods
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
    
    // Log the result
    String result;
    for (int i = 0; i < 8; i++) {
        if (i > 0) result += ":";
        result += String(address[i], HEX);
    }
    Logger::debug("Converted address: " + result);
}

// Relay Name Methods
bool PreferencesManager::setRelayName(uint8_t relayId, const char* name) {
    if (!prefsMutex || !prefs || relayId > 1) return false;
    
    bool success = false;
    if (xSemaphoreTake(prefsMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        String key = "relay_" + String(relayId);
        success = prefs->putString(key.c_str(), name);
        Logger::debug("Relay name " + String(success ? "updated" : "update failed") + 
                     " for Relay " + String(relayId));
        xSemaphoreGive(prefsMutex);
    }
    return success;
}

String PreferencesManager::getRelayName(uint8_t relayId) {
    if (relayId > 1) return "";
    
    String name = "";
    if (prefsMutex && prefs && xSemaphoreTake(prefsMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        String key = "relay_" + String(relayId);
        name = prefs->getString(key.c_str(), "");
        xSemaphoreGive(prefsMutex);
    }
    return name;
}

void PreferencesManager::getDisplaySensor(uint8_t* address) {
    if (!prefsMutex || !prefs) {
        Logger::error("Preferences not initialized in getDisplaySensor");
        memset(address, 0, 8);
        return;
    }
    
    if (xSemaphoreTake(prefsMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        String addrStr = prefs->getString("display_sensor", "0000000000000000");
        Logger::debug("Retrieved display sensor from preferences: " + addrStr);
        
        stringToAddress(addrStr.c_str(), address);
        
        // Log the converted address
        String convertedAddr = addressToString(address);
        Logger::debug("Converted display sensor address: " + convertedAddr);
        
        xSemaphoreGive(prefsMutex);
    } else {
        Logger::error("Failed to acquire mutex in getDisplaySensor");
        memset(address, 0, 8);
    }
}

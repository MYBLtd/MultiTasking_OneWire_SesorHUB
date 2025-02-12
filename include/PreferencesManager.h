#pragma once

#include <Arduino.h>
#include "SharedDefinitions.h"
#include "PreferenceStorage.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "Logger.h"

class PreferencesManager {
public:
    // Core functionality
    static void init();
    static void reset();
    static void printCurrentPreferences();
    
    // Credentials Management
    static bool setCredential(const char* key, const char* value);
    static String getCredential(const char* key);
    static bool hasCredential(const char* key);
    static bool removeCredential(const char* key);
    
    // MQTT Configuration
    static bool setMqttConfig(const char* server, uint16_t port, 
                            const char* username, const char* password);
    static void getMqttConfig(char* server, unsigned short& port, 
                            char* username, char* password);
    static bool isMqttConfigured();
    static bool clearMqttConfig();
    
    // OneWire Bus Configuration
    static void setAutoScanEnabled(bool enabled);
    static bool getAutoScanEnabled();
    static void setScanInterval(uint32_t seconds);
    static uint32_t getScanInterval();
    
    // Sensor Management
    static bool setSensorName(const uint8_t* address, const char* name);
    static String getSensorName(const uint8_t* address);
    static bool setDisplaySensor(const uint8_t* address);
    static void getDisplaySensor(uint8_t* address);
    static bool setRelayName(uint8_t relayId, const char* name);
    static String getRelayName(uint8_t relayId);
    
    // Utility methods
    static String addressToString(const uint8_t* address);
    static void stringToAddress(const String& str, uint8_t* address);

private:
    static PreferenceStorage* prefs;
    static SemaphoreHandle_t prefsMutex;
    
    // Mutex management
    static bool acquireMutex(const char* caller);
    static void releaseMutex();
    
    // Helper methods
    static String getSensorKey(const uint8_t* address);
    static bool isInitialized();
    
    // Prevent instantiation
    PreferencesManager() = delete;
};

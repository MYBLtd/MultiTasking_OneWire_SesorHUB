#pragma once

#include <ArduinoJson.h>
#include "PreferencesManager.h"
#include "OneWireManager.h"
#include "Logger.h"
#include "SharedDefinitions.h"

// Forward declaration to avoid circular dependency
class OneWireManager;

class PreferencesApiHandler {
public:
    PreferencesApiHandler(OneWireManager& owManager) 
        : oneWireManager(owManager) {}
        
    String handleGet();
    bool handlePost(const String& jsonData);

private:
    OneWireManager& oneWireManager;

    bool validateMqttConfig(JsonObject& mqtt);
    bool validateScanningConfig(JsonObject& scanning);
    bool validateDisplayConfig(JsonObject& display);
    bool validateSensorName(const char* name);
    bool validateHostname(const char* hostname);

    void addMqttConfigToJson(JsonObject& root);
    void addScanningConfigToJson(JsonObject& root);
    void addDisplayConfigToJson(JsonObject& root);
    void addSensorNamesToJson(JsonObject& root);

    bool updateMqttConfig(JsonObject& mqtt);
    bool updateScanningConfig(JsonObject& scanning);
    bool updateDisplayConfig(JsonObject& display);
    bool updateSensorNames(JsonVariant sensors);
};
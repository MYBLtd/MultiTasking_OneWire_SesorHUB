#pragma once

#include <Arduino.h>
#include "SystemTypes.h"
#include "OneWireManager.h"
#include "WebServer.h"
#include "MqttManager.h"
#include "Config.h"
#include "OneWireTask.h"

class NetworkTask {
public:
    static void init();
    static void start();
    
    // MQTT publishing methods
    static void publishTemperature(const char* sensorName, float temperature);
    static void publishRelayState(uint8_t relayId, bool state);
    static bool publishToTopic(const char* topic, const char* payload);
    static void publishSensorBatch(const std::vector<TemperatureSensor>& sensors, size_t startIdx, size_t count);
    static bool maintainConnection();  // Add this line
    
private:
    static MqttManager mqttManager;
    static OneWireManager& owManager;
    static WebServer webServer;
    static QueueHandle_t publishQueue;
    static QueueHandle_t controlQueue;
    static unsigned long lastPublishTime;  // Changed from TickType_t to unsigned long
    
    static void taskFunction(void* parameter);
    static bool publishSensorData(const TemperatureSensor& sensor);
    static String addressToString(const uint8_t* address);
};

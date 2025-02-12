#pragma once

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ETH.h>
#include "Logger.h"
#include "PreferencesManager.h"
#include "Config.h"
#include "certificates.h"
#include "SystemTypes.h"

class MqttManager {
public:
    MqttManager();
    
    // Core public interface
    void begin();
    bool maintainConnection();  // Changed return type to bool
    bool connected();
    
    // Publishing methods
    bool publish(const char* topic, const char* payload, bool retained = true);
    void publishSensorData(const TemperatureSensor& sensor);
    void publishRelayState(uint8_t relayId, bool state);  // New method
    void publishAuxDisplayData(const TemperatureSensor& sensor);  // New method
    void setServer(const IPAddress& ip);  // Add this line

private:
    // Network clients
    WiFiClientSecure wifiClient;  // Use WiFiClientSecure since you're using setCACert
    PubSubClient mqtt;
    
    // Connection configuration
    String mqttBroker;
    uint16_t mqttPort;
    String mqttUsername;
    String mqttPassword;
    
    // Connection state
    unsigned long lastReconnectAttempt;
    unsigned long lastPublishAttempt;
    unsigned int currentReconnectDelay;

    // Private methods
    void setupSecureClient();
    void loadConfiguration();
    bool reconnect();
    unsigned int getReconnectDelay();
    
    // Constants for timing and retries
    static constexpr unsigned int INITIAL_RECONNECT_DELAY = 1000;
    static constexpr unsigned int MAX_RECONNECT_DELAY = 60000;
    static constexpr unsigned int PUBLISH_RATE_LIMIT = 100;
    static constexpr unsigned int RECONNECT_INTERVAL = 5000;
    static constexpr uint8_t MQTT_QOS = 1;
};

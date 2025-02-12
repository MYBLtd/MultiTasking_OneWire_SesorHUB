// MqttManager.cpp
#include <algorithm>
#include "MqttManager.h"
#include <cstring>
#include "PreferencesManager.h"

MqttManager::MqttManager() 
    : wifiClient()
    , mqtt(wifiClient)  // Initialize mqtt with wifiClient
    , mqttBroker("")
    , mqttPort(0)
    , mqttUsername("")
    , mqttPassword("")
    , lastReconnectAttempt(0)
    , lastPublishAttempt(0)
    , currentReconnectDelay(0) {
    
    setupSecureClient();
}

void MqttManager::begin() {
    Logger::info("Initializing MQTT Manager", Logger::Category::NETWORK);
    loadConfiguration();
}

bool MqttManager::connected() {
    return mqtt.connected();
}

// Main connection maintenance function
bool MqttManager::maintainConnection() {
    if (!connected()) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt >= RECONNECT_INTERVAL) {
            bool reconnectSuccess = reconnect();
            lastReconnectAttempt = now;
            return reconnectSuccess;
        }
        return false;
    }
    
    mqtt.loop();
    return true;
}
void MqttManager::loadConfiguration() {
    // Create temporary buffers for loading the configuration
    char broker[MAX_MQTT_SERVER_LENGTH] = {0};
    char username[MAX_MQTT_CRED_LENGTH] = {0};
    char password[MAX_MQTT_CRED_LENGTH] = {0};
    unsigned short port = 0;
    
    // Load configuration from preferences
    PreferencesManager::getMqttConfig(broker, port, username, password);
    
    // Store configuration in class members
    mqttBroker = String(broker);
    mqttPort = port;
    mqttUsername = String(username);
    mqttPassword = String(password);
    
    // Update MQTT client configuration if we have valid settings
    if (mqttBroker.length() > 0 && mqttPort > 0) {
        mqtt.setServer(mqttBroker.c_str(), mqttPort);
        Logger::info("MQTT configured with broker: " + mqttBroker + ":" + String(mqttPort), 
                    Logger::Category::NETWORK);
    } else {
        Logger::warning("MQTT not configured - check settings", Logger::Category::NETWORK);
    }
}

void MqttManager::setupSecureClient() {
    // Configure secure WiFi client with the root CA certificate
    //wifiClient.setCACert(letsencrypt_root_ca);
    wifiClient.setCACert(getLetsEncryptRootCA());
    
    // Configure MQTT client
    mqtt.setBufferSize(8192);  // Set a reasonably large buffer for sensor data
    mqtt.setSocketTimeout(10);  // 10 seconds socket timeout
    
    // Load and apply MQTT configuration
    char broker[MAX_MQTT_SERVER_LENGTH];
    char username[MAX_MQTT_CRED_LENGTH];
    char password[MAX_MQTT_CRED_LENGTH];
    unsigned short port;
    
    PreferencesManager::getMqttConfig(broker, port, username, password);
    
    if (strlen(broker) > 0 && port > 0) {
        mqtt.setServer(broker, port);
        Logger::debug("MQTT client configured with broker: " + String(broker));
    }
}

bool MqttManager::publish(const char* topic, const char* payload, bool retained) {
    if (!connected()) {
        Logger::warning("Not publishing - MQTT disconnected");
        return false;
    }
    
    // Add retry mechanism with exponential backoff
    const int maxRetries = 3;
    for (int retry = 0; retry < maxRetries; retry++) {
        if (retry > 0) {
            // Exponential backoff between retries
            delay((1 << retry) * 200);  // 200ms, 400ms, 600ms
        }

        if (mqtt.publish(topic, payload, retained)) {
            // Give some time for the message to be processed
            delay(50);
            return true;
        }
        
        Logger::warning("Publish attempt " + String(retry + 1) + " failed for topic: " + String(topic));
    }
    
    return false;
}

void MqttManager::publishRelayState(uint8_t relayId, bool state) {
    if (!connected()) {
        Logger::warning("Not publishing relay state - MQTT disconnected");
        return;
    }

    char topicBuffer[128];
    const char* stateStr = state ? "ON" : "OFF";
    
    // Publish state topic
    snprintf(topicBuffer, sizeof(topicBuffer), 
             "%s/%s/relay/%d/state",
             SYSTEM_NAME, DEVICE_ID, relayId + 1);
    
    Logger::debug("Publishing relay " + String(relayId + 1) + " state: " + stateStr);
    publish(topicBuffer, stateStr, true);
    
    // Publish availability topic
    snprintf(topicBuffer, sizeof(topicBuffer), 
             "%s/%s/relay/%d/availability",
             SYSTEM_NAME, DEVICE_ID, relayId + 1);
    
    publish(topicBuffer, "online", true);
}

void MqttManager::publishSensorData(const TemperatureSensor& sensor) {
    if (!connected()) {
        Logger::warning("Not publishing sensor data - MQTT disconnected");
        return;
    }

    char topicBuffer[128];
    char payloadBuffer[64];
    String sensorId = PreferencesManager::addressToString(sensor.address);  // Use PreferencesManager's method


    // Publish temperature
    snprintf(topicBuffer, sizeof(topicBuffer), 
             "%s/%s/%s/%s/temperature",
             SYSTEM_NAME, DEVICE_ID, MQTT_TOPIC_BASE, sensorId.c_str());
    
    snprintf(payloadBuffer, sizeof(payloadBuffer), "%.2f", sensor.temperature);
    publish(topicBuffer, payloadBuffer, true);

    // Publish status
    snprintf(topicBuffer, sizeof(topicBuffer), 
             "%s/%s/%s/%s/status",
             SYSTEM_NAME, DEVICE_ID, MQTT_TOPIC_BASE, sensorId.c_str());
    
    publish(topicBuffer, sensor.valid ? "online" : "error", true);

    // Publish last update time
    snprintf(topicBuffer, sizeof(topicBuffer), 
             "%s/%s/%s/%s/last_update",
             SYSTEM_NAME, DEVICE_ID, MQTT_TOPIC_BASE, sensorId.c_str());
    
    snprintf(payloadBuffer, sizeof(payloadBuffer), "%lu", sensor.lastReadTime);
    publish(topicBuffer, payloadBuffer, true);
}

void MqttManager::publishAuxDisplayData(const TemperatureSensor& sensor) {
    String topic = String(SYSTEM_NAME) + "/" + 
                  String(DEVICE_ID) + "/" + 
                  String(MQTT_AUX_DISPLAY_TOPIC);
    
    String payload = String(sensor.temperature);
    mqtt.publish(topic.c_str(), payload.c_str(), true);
    Logger::debug("Published aux display temperature: " + payload + " to topic: " + topic);
}

// Private helper methods
bool MqttManager::reconnect() {
    if (!ETH.linkUp()) {
        Logger::info("Network not ready - skipping MQTT reconnection", Logger::Category::NETWORK);
        return false;
    }
    
    // Verify we have configuration
    if (mqttBroker.isEmpty() || mqttPort == 0) {
        Logger::warning("MQTT not configured - cannot reconnect", Logger::Category::NETWORK);
        return false;
    }
    
    currentReconnectDelay = getReconnectDelay();
    
    Logger::info("Attempting MQTT connection...", Logger::Category::NETWORK);
    
    String clientId = "ESP32-";
    clientId += ETH.macAddress();
    
    if (mqtt.connect(clientId.c_str(), 
                    mqttUsername.c_str(), 
                    mqttPassword.c_str(),
                    "status", 
                    MQTT_QOS, 
                    true, 
                    "offline")) {
        Logger::info("MQTT Connected successfully", Logger::Category::NETWORK);
        currentReconnectDelay = 0;  // Reset delay on success
        char topicBuffer[128];
        
        for (int i = 0; i < 2; i++) {
            snprintf(topicBuffer, sizeof(topicBuffer), 
                     "%s/%s/%s/relay%d/set",
                     SYSTEM_NAME, DEVICE_ID, MQTT_SWITCH_BASE, i + 1);
            mqtt.subscribe(topicBuffer);
        }

        mqtt.publish("status", "online", true);
        return true;
    }
    
    // Create a properly formatted error message
    char message[64];
    snprintf(message, sizeof(message), "MQTT connection failed, rc=%d", mqtt.state());
    Logger::error(message, Logger::Category::NETWORK);
    return false;
}

unsigned int MqttManager::getReconnectDelay() {
    return currentReconnectDelay == 0 ? 
           INITIAL_RECONNECT_DELAY : 
           min(currentReconnectDelay * 2, MAX_RECONNECT_DELAY);
}

void MqttManager::setServer(const IPAddress& ip) {
    mqtt.setServer(ip.toString().c_str(), mqttPort);  // Using the internal PubSubClient instance
    Logger::debug("MQTT server IP updated to: " + ip.toString());
}
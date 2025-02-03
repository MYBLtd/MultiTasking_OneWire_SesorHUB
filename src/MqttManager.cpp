// MqttManager.cpp

#include "MqttManager.h"
#include <cstring>

MqttManager::MqttManager() 
    : wifiClient()
    , mqttClient(wifiClient)
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
    return mqttClient.connected();
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
    
    mqttClient.loop();
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
        mqttClient.setServer(mqttBroker.c_str(), mqttPort);
        Logger::info("MQTT configured with broker: " + mqttBroker + ":" + String(mqttPort), 
                    Logger::Category::NETWORK);
    } else {
        Logger::warning("MQTT not configured - check settings", Logger::Category::NETWORK);
    }
}

void MqttManager::setupSecureClient() {
    // Configure secure WiFi client with the root CA certificate
    wifiClient.setCACert(letsencrypt_root_ca);
    
    // Configure MQTT client
    mqttClient.setBufferSize(8192);  // Set a reasonably large buffer for sensor data
    mqttClient.setSocketTimeout(10);  // 10 seconds socket timeout
    
    // Load and apply MQTT configuration
    char broker[MAX_MQTT_SERVER_LENGTH];
    char username[MAX_MQTT_CRED_LENGTH];
    char password[MAX_MQTT_CRED_LENGTH];
    unsigned short port;
    
    PreferencesManager::getMqttConfig(broker, port, username, password);
    
    if (strlen(broker) > 0 && port > 0) {
        mqttClient.setServer(broker, port);
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

        if (mqttClient.publish(topic, payload, retained)) {
            // Give some time for the message to be processed
            delay(50);
            return true;
        }
        
        Logger::warning("Publish attempt " + String(retry + 1) + " failed for topic: " + String(topic));
    }
    
    return false;
}

void MqttManager::publishSensorData(const TemperatureSensor& sensor) {
    if (!connected()) {
        Logger::warning("Not publishing sensor data - MQTT disconnected");
        return;
    }

    // Reduced buffer sizes - most payloads are under 100 bytes
    char statePayload[256];
    char topicBuffer[128];
    char sensorId[17];

    // Create more compact JSON to reduce payload size
    snprintf(statePayload, sizeof(statePayload), 
             "{\"t\":%.2f,\"ts\":%lu,\"s\":\"%s\"}",
             sensor.temperature,
             sensor.lastReadTime,
             sensor.valid ? "1" : "0");
    
    // Create the sensor ID
    snprintf(sensorId, sizeof(sensorId), "%02X%02X%02X%02X%02X%02X%02X%02X",
             sensor.address[0], sensor.address[1], sensor.address[2], sensor.address[3],
             sensor.address[4], sensor.address[5], sensor.address[6], sensor.address[7]);
             
    // Create topic
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/%s/%s/state",
             SYSTEM_NAME, MQTT_TOPIC_BASE, sensorId);
    
    // Give more time for stack frames to unwind
    delay(50);  
    
    bool success = publish(topicBuffer, statePayload, true);
    
    if (!success) {
        Logger::error("Failed to publish data for sensor " + String(sensorId));
    }
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
    
    if (mqttClient.connect(clientId.c_str(), 
                          mqttUsername.c_str(), 
                          mqttPassword.c_str(),
                          "status", 
                          MQTT_QOS, 
                          true, 
                          "offline")) {
        Logger::info("MQTT Connected successfully", Logger::Category::NETWORK);
        currentReconnectDelay = 0;  // Reset delay on success
        
        mqttClient.publish("status", "online", true);
        return true;
    }
    
    // Create a properly formatted error message
    char message[64];
    snprintf(message, sizeof(message), "MQTT connection failed, rc=%d", mqttClient.state());
    Logger::error(message, Logger::Category::NETWORK);
    return false;
}

unsigned int MqttManager::getReconnectDelay() {
    return currentReconnectDelay == 0 ? 
           INITIAL_RECONNECT_DELAY : 
           min(currentReconnectDelay * 2, MAX_RECONNECT_DELAY);
}
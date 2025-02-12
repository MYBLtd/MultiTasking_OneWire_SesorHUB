// NetworkTask.cpp
#include <algorithm>
#include "NetworkTask.h"
#include "SystemHealth.h"
#include "OneWireManager.h"
#include "Logger.h"
#include <ETH.h>
#include <ESPmDNS.h>
#include "Config.h"
#include "ControlTask.h"

#define NETWORK_TASK_STACK_SIZE 16192
#define SENSOR_BATCH_SIZE 4        // Process sensors in small batches
#define BATCH_DELAY_MS 500         // Time between batches to let memory stabilize
#define SENSOR_DELAY_MS 100        // Time between individual sensors

// Static member initializations
MqttManager NetworkTask::mqttManager;
OneWireManager& NetworkTask::owManager = OneWireTask::manager;
WebServer NetworkTask::webServer(NetworkTask::owManager);
QueueHandle_t NetworkTask::publishQueue = nullptr;
QueueHandle_t NetworkTask::controlQueue = nullptr;
unsigned long NetworkTask::lastPublishTime = 0;

void NetworkTask::init() {
    Logger::info("Starting Network task initialization");
    
    publishQueue = xQueueCreate(20, sizeof(TaskMessage));
    controlQueue = xQueueCreate(10, sizeof(TaskMessage));
    
    if (!publishQueue || !controlQueue) {
        Logger::error("Failed to create queues");
        return;
    }
    Logger::info("Network queues created");

    mqttManager.begin();
    Logger::info("MQTT Manager initialized");
    
    webServer.begin();
    Logger::info("Web server started");
    
    Logger::info("Network initialization complete");
}

void NetworkTask::start() {
    xTaskCreate(
        taskFunction,
        "NetworkTask",
        NETWORK_TASK_STACK_SIZE,
        nullptr,
        NETWORK_TASK_PRIORITY,
        nullptr
    );
}

String NetworkTask::addressToString(const uint8_t* address) {
    if (!address) {
        return String("Invalid");
    }
    
    char buffer[17];
    snprintf(buffer, sizeof(buffer), "%02X%02X%02X%02X%02X%02X%02X%02X",
             address[0], address[1], address[2], address[3],
             address[4], address[5], address[6], address[7]);
    return String(buffer);
}

void NetworkTask::publishSensorBatch(const std::vector<TemperatureSensor>& sensors, 
                                   size_t startIdx, 
                                   size_t count) {
    // Ensure we don't exceed vector bounds
    size_t endIdx = std::min(startIdx + count, sensors.size());
    
    for (size_t i = startIdx; i < endIdx; i++) {
        if (!mqttManager.connected()) {
            Logger::error("Lost MQTT connection during batch publishing");
            return;
        }
        
        mqttManager.publishSensorData(sensors[i]);
        vTaskDelay(pdMS_TO_TICKS(SENSOR_DELAY_MS));
    }
    
    // Allow system to stabilize between batches
    vTaskDelay(pdMS_TO_TICKS(BATCH_DELAY_MS));
}

void NetworkTask::taskFunction(void* parameter) {
    TickType_t lastWakeTime = xTaskGetTickCount();
    
    if (MDNS.begin(MDNS_HOSTNAME)) {
        MDNS.addService("http", "tcp", 80);
        MDNS.addServiceTxt("http", "tcp", "name", MDNS_HOSTNAME);
        Logger::info("mDNS responder started");
    } else {
        Logger::error("Error setting up mDNS responder!");
    }
    
    while (true) {
        unsigned long currentTime = millis();
        bool mqttIsConnected = mqttManager.maintainConnection();
        
        // Handle sensor publishing with batching
        if ((currentTime - lastPublishTime) >= MQTT_PUBLISH_INTERVAL) {
            if (mqttIsConnected && mqttManager.connected()) {
                const auto& sensors = owManager.getSensorList();
                
                // First, explicitly handle the display sensor
                uint8_t displaySensorAddr[8];
                PreferencesManager::getDisplaySensor(displaySensorAddr);
                bool displaySensorHandled = false;
                
                // Find and publish display sensor separately from batches
                for (const auto& sensor : sensors) {
                    if (memcmp(sensor.address, displaySensorAddr, sizeof(displaySensorAddr)) == 0) {
                        char tempStr[10];
                        snprintf(tempStr, sizeof(tempStr), "%.1f", sensor.temperature);
                        if (NetworkTask::publishToTopic(MQTT_AUX_DISPLAY_TOPIC, tempStr)) {
                            Logger::debug("Published display sensor temperature: " + String(tempStr));
                        }
                        displaySensorHandled = true;
                        break;
                    }
                }
                
                if (!displaySensorHandled) {
                    Logger::warning("Display sensor not found in sensor list");
                }
                
                // Then handle all other sensors in batches
                size_t totalSensors = sensors.size();
                
                Logger::info("Starting publication cycle for " + String(totalSensors) + 
                           " sensors in batches of " + String(SENSOR_BATCH_SIZE));
                
                // Calculate number of complete batches
                size_t numBatches = (totalSensors + SENSOR_BATCH_SIZE - 1) / SENSOR_BATCH_SIZE;
                
                // Process all batches including the last partial one
                for (size_t batchIdx = 0; batchIdx < numBatches; batchIdx++) {
                    size_t startIdx = batchIdx * SENSOR_BATCH_SIZE;
                    size_t batchSize = std::min<size_t>(SENSOR_BATCH_SIZE, totalSensors - startIdx);
                    publishSensorBatch(sensors, startIdx, batchSize);
                    
                    // Also publish relay states after the first batch
                    if (batchIdx == 0) {
                        mqttManager.publishRelayState(0, ControlTask::getRelayState(0));
                        mqttManager.publishRelayState(1, ControlTask::getRelayState(1));
                    }
                }
                
                lastPublishTime = millis();
                Logger::info("Completed publication cycle");
            } else {
                Logger::warning("Skipping publication cycle - MQTT not connected");
                lastPublishTime = currentTime;
            }
        }
        
        // Process queued messages
        TaskMessage msg;
        while (xQueueReceive(publishQueue, &msg, 0) == pdTRUE) {
            if (mqttManager.connected()) {
                switch (msg.type) {
                    case MessageType::MQTT_PUBLISH:
                        mqttManager.publish(msg.data.mqttPublish.topic, 
                                         msg.data.mqttPublish.payload, 
                                         true);
                        break;
                    default:
                        Logger::warning("Unknown message type in Network task");
                        break;
                }
            }
        }
        
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(100));
    }
}

bool NetworkTask::publishSensorData(const TemperatureSensor& sensor) {
    bool success = true;
    
    if (sensor.valid) {
        char tempStr[10];
        snprintf(tempStr, sizeof(tempStr), "%.2f", sensor.temperature);
        success &= mqttManager.publish((String(SYSTEM_NAME) + "/" + MQTT_TOPIC_BASE + "/" + 
                                     addressToString(sensor.address) + "/temperature").c_str(), 
                                     tempStr, true);
        
        if (!success) {
            Logger::error("Failed to publish temperature for sensor " + 
                         addressToString(sensor.address));
        }
    }
    
    char timeStr[20];
    snprintf(timeStr, sizeof(timeStr), "%lu", sensor.lastReadTime);
    success &= mqttManager.publish((String(SYSTEM_NAME) + "/" + MQTT_TOPIC_BASE + "/" + 
                                 addressToString(sensor.address) + "/last_update").c_str(), 
                                 timeStr, true);
    
    const char* status = sensor.valid ? "online" : "error";
    success &= mqttManager.publish((String(SYSTEM_NAME) + "/" + MQTT_TOPIC_BASE + "/" + 
                                 addressToString(sensor.address) + "/status").c_str(), 
                                 status, true);
    
    return success;
}

bool NetworkTask::publishToTopic(const char* topic, const char* payload) {
    if (!mqttManager.connected()) {
        Logger::warning("MQTT not connected - cannot publish to " + String(topic));
        return false;
    }
    
    // Build the full topic path: system_name/device_id/topic
    String fullTopic = String(SYSTEM_NAME) + "/" + DEVICE_ID + "/" + topic;
    Logger::debug("Publishing to topic: " + fullTopic);
    
    if (mqttManager.publish(fullTopic.c_str(), payload, true)) {
        Logger::debug("Successfully published: " + String(payload));
        return true;
    } else {
        Logger::error("Failed to publish to topic: " + fullTopic);
        return false;
    }
}

bool NetworkTask::maintainConnection() {
    if (!ETH.linkUp()) {
        Logger::error("Ethernet link down");
        return false;
    }

    // Log DNS configuration
    Logger::info("DNS Server: " + ETH.dnsIP().toString());
    
    // Try DNS resolution with retry
    IPAddress mqttIP;
    const int DNS_RETRY_COUNT = 3;
    const int DNS_RETRY_DELAY = 1000;
    
    for (int i = 0; i < DNS_RETRY_COUNT; i++) {
        Logger::debug("DNS lookup attempt " + String(i + 1) + " for mq.cemco.nl");
        
        if (WiFi.hostByName("mq.cemco.nl", mqttIP)) {
            Logger::info("DNS resolved mq.cemco.nl to " + mqttIP.toString());
            // Update MQTT broker IP
            mqttManager.setServer(mqttIP);
            return true;
        }
        
        Logger::warning("DNS lookup failed, attempt " + String(i + 1) + 
                       ", DNS Server: " + ETH.dnsIP().toString());
        delay(DNS_RETRY_DELAY);
    }
    
    Logger::error("All DNS lookups failed after " + String(DNS_RETRY_COUNT) + " attempts");
    return false;
}
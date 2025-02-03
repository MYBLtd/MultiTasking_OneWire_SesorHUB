// SystemTypes.h
#pragma once

#include <stdint.h>
#include <Arduino.h>
#include "config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

// Message types for inter-task communication
enum class MessageType : uint8_t {
    RELAY_CHANGE_REQUEST,
    TEMPERATURE_UPDATE,
    SENSOR_SCAN_REQUEST,
    MQTT_PUBLISH
};

// MQTT message structure
struct MqttPublishData {
    char topic[64];     // Topic buffer
    char payload[128];  // Message payload buffer
};

// Message data union
union MessageData {
    struct {
        uint8_t relayId;
        bool state;
    } relayChange;
    
    struct {
        uint8_t sensorIndex;
        float temperature;
    } temperatureUpdate;
    
    MqttPublishData mqttPublish;  // Added MQTT publish data
};

// Message for inter-task communication
struct TaskMessage {
    MessageType type;
    MessageData data;
};

// Rest of your existing types...
struct RelayState {
    bool requested;
    bool actual;
    uint32_t lastChangeTime;
};

// Device status enumeration
enum class DeviceStatus : uint8_t {
    OK = 0,
    ERROR = 1,
    DISCONNECTED = 2,
    INITIALIZING = 3
};

// Sensor types enumeration
enum class SensorType : uint8_t {
    TEMPERATURE = 0,
    HUMIDITY = 1,
    PRESSURE = 2,
    UNKNOWN = 255
};

// Display mode enumeration
enum class DisplayMode : uint8_t {
    NORMAL = 0,
    ERROR = 1,
    TEST = 2,
    OFF = 3
};

// Temperature scale enumeration
enum class TemperatureScale : uint8_t {
    CELSIUS = 0,
    FAHRENHEIT = 1,
    KELVIN = 2
};

// Temperature sensor data structure
struct TemperatureSensor {
    uint8_t address[8];                              // Sensor's unique address
    char friendlyName[MAX_FRIENDLY_NAME_LENGTH];     // Human-readable name
    float temperature;                               // Current temperature reading
    float lastValidReading;                         // Last known good reading
    uint32_t lastReadTime;                          // Timestamp of last reading
    uint8_t consecutiveErrors;                      // Error tracking
    bool isActive;                                  // Whether sensor is currently responding
    bool valid;                                     // Whether current reading is valid
};

// Sensor data structure
struct SensorData {
    float value;
    SensorType type;
    uint32_t timestamp;
    DeviceStatus status;
    char friendlyName[MAX_FRIENDLY_NAME_LENGTH];
};

// System status structure
struct SystemStatus {
    DeviceStatus deviceStatus;
    bool networkConnected;
    bool mqttConnected;
    uint32_t uptime;
    uint32_t lastError;
    DisplayMode displayMode;
};
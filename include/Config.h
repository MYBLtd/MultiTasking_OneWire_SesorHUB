// Config.h
#pragma once
#include <Arduino.h>
#include <stdint.h>

#define DEVICE_NAME "Chaoticvolt_SensorHUB"
#define FIRMWARE_VERSION "1.2.3"

// Network Configuration
#define MDNS_HOSTNAME "sensorhub"
//#define MDNS_SERVER_NAME "Temperature Monitor"

// MQTT Broker Configuration
#define SYSTEM_NAME "chaoticvolt"
#define DEVICE_ID "sensorhub1"
#define MQTT_TOPIC_BASE "sensors"
#define MQTT_SWITCH_BASE "switch"
#define MQTT_STATE_TOPIC "status"
#define MQTT_AVAILABILITY_TOPIC "availability"
#define MQTT_SET_TOPIC "set"
#define MQTT_AUX_DISPLAY_TOPIC "sensors/BabelSensor"
// Pin Configuration
constexpr uint8_t ONE_WIRE_BUS = 4;

// System Configuration
constexpr size_t MAX_ONEWIRE_SENSORS = 16;
constexpr uint32_t WATCHDOG_TIMEOUT = 30000;  // 30 seconds

// Task Stack Sizes
constexpr uint32_t ONEWIRE_TASK_STACK_SIZE = 4096;
constexpr uint32_t NETWORK_TASK_STACK_SIZE = 8192;
constexpr uint32_t CONTROL_TASK_STACK_SIZE = 4096;

// Task Priorities
constexpr uint8_t ONEWIRE_TASK_PRIORITY = 3;
constexpr uint8_t NETWORK_TASK_PRIORITY = 2;
constexpr uint8_t CONTROL_TASK_PRIORITY = 2;

// Timing Intervals (ms)
constexpr uint32_t SCAN_INTERVAL = 30000;           // Scan for new sensors every 30 seconds
constexpr uint32_t READ_INTERVAL = 10000;           // Read temperatures every 10 seconds
constexpr uint32_t WEB_UPDATE_INTERVAL = 2000;      // Update web interface every 2 seconds
constexpr uint32_t MQTT_PUBLISH_INTERVAL = 5000;    // Update web interface every 2 seconds
constexpr uint32_t TASK_INTERVAL = 1000;            // Task loop interval 1 second
constexpr uint32_t DISPLAY_UPDATE_INTERVAL = 1000;

// System Requirements
constexpr size_t MINIMUM_REQUIRED_HEAP = 32768;

// MQTT Configuration
#ifndef MQTT_MAX_PACKET_SIZE
constexpr size_t MQTT_MAX_PACKET_SIZE = 512;
#endif

// System Configuration
#define MAX_FRIENDLY_NAME_LENGTH 32

// display pins
// Safe configuration avoiding all used peripherals
#define DISPLAY_DIO  14   // HS2_CLK - safe if not using SD
#define DISPLAY_CLK  17   // CS - safe if not using SPI

// pins for the relays
#define RELAY_1_PIN 32
#define RELAY_2_PIN 33

#define ETH_TYPE               ETH_PHY_LAN8720
#define ETH_ADDR               1
#define ETH_POWER_PIN          16
#define ETH_MDC_PIN           23
#define ETH_MDIO_PIN          18
#define ETH_PHY_ADDR           0
#define ETH_CLK_MODE          ETH_CLOCK_GPIO0_IN
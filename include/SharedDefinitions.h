// SharedDefinitions.h
#pragma once

// Include necessary standard headers
#include <cstddef>   // For size_t
#include <cstdint>   // For uint32_t and other integer types
#include <Arduino.h> // For additional Arduino-specific types

// Scan intervals (in seconds)
constexpr uint32_t DEFAULT_SCAN_INTERVAL = 60;    // Default interval between scans
constexpr uint32_t MIN_SCAN_INTERVAL = 10;        // Minimum allowed interval
constexpr uint32_t MAX_SCAN_INTERVAL = 3600;      // Maximum allowed interval (1 hour)

// Storage size limits
constexpr size_t MAX_SENSOR_NAME_LENGTH = 32;     // Maximum length for sensor names
constexpr size_t MAX_MQTT_SERVER_LENGTH = 64;     // Maximum length for MQTT broker address
constexpr size_t MAX_MQTT_CRED_LENGTH = 32;       // Maximum length for MQTT credentials


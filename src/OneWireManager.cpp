// OneWireManager.cpp
// This class manages communication with DS18B20 temperature sensors on the OneWire bus.
// It provides thread-safe access to sensor data and handles device discovery.

#include "Config.h"
#include "OneWireManager.h"
#include "Logger.h"
#include <algorithm>

// Constructor takes the OneWire bus pin and initializes the system
OneWireManager::OneWireManager(uint8_t pin) 
    : oneWire(pin)
    , sensors(&oneWire)
    , busyFlag(false)
    , sensorMutex(nullptr)
    , lastScanTime(0)
    , lastReadTime(0)
    , conversionStartTime(0)
    , conversionInProgress(false) {
    
    // Create mutex for thread-safe access
    sensorMutex = xSemaphoreCreateMutex();
    if (!sensorMutex) {
        Logger::error("Failed to create sensor mutex in constructor");
        return;
    }
    
    // Initialize hardware with proper configuration
    pinMode(pin, INPUT_PULLUP);
    vTaskDelay(pdMS_TO_TICKS(100));  // Allow bus to stabilize
    sensors.begin();
    
    // Configure for efficient operation with multiple sensors
    sensors.setWaitForConversion(false);  // Enable async operation
    sensors.setResolution(12);  // Set precision to 12 bits (0.0625°C)
    
    Logger::info("OneWire bus initialized on pin " + String(pin));
}

// Start a temperature conversion for all sensors simultaneously
void OneWireManager::startTemperatureConversion() {
    if (!verifyMutex() || isBusBusy()) {
        Logger::warning("Cannot start conversion - bus busy or mutex invalid");
        return;
    }
    
    setBusBusy(true);
    
    // Request temperature conversion for all sensors at once
    sensors.requestTemperatures();
    conversionStartTime = millis();
    conversionInProgress = true;
    
    setBusBusy(false);
    Logger::debug("Started temperature conversion for all sensors");
}

// Check if conversion is complete and collect temperatures if ready
bool OneWireManager::checkAndCollectTemperatures() {
    if (!verifyMutex() || !sensorMutex) return false;
    
    if (xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        Logger::error("Failed to acquire mutex in checkAndCollectTemperatures");
        return false;
    }
    
    bool success = true;
    std::vector<TemperatureSensor> updatedList;
    updatedList.reserve(sensorList.size());
    
    for (const auto& sensor : sensorList) {
        TemperatureSensor updated = sensor;
        float temp = sensors.getTempC(sensor.address);
        
        if (temp != DEVICE_DISCONNECTED_C && temp != 85.0) {
            updated.temperature = temp;
            updated.lastValidReading = temp;
            updated.lastReadTime = millis();
            updated.valid = true;
            updated.consecutiveErrors = 0;
        } else {
            updated.consecutiveErrors++;
            if (updated.consecutiveErrors > MAX_RETRIES) {
                updated.valid = false;
            }
            // Keep last valid reading but mark as invalid
            updated.temperature = updated.lastValidReading;
            success = false;
        }
        updatedList.push_back(std::move(updated));
    }
    
    sensorList = std::move(updatedList);
    conversionInProgress = false;
    
    xSemaphoreGive(sensorMutex);
    return success;
}

// Enhanced bus scanning with better error handling and retry logic
bool OneWireManager::scanDevices() {
    if (isBusBusy()) {
        Logger::warning("Cannot scan - bus is busy");
        return false;
    }
    
    setBusBusy(true);
    std::vector<TemperatureSensor> tempList;
    bool scanSuccess = false;
    
    try {
        Logger::info("Starting OneWire bus scan...");
        
        for (int retry = 0; retry < MAX_RETRIES; retry++) {
            sensors.begin();  // Reset the bus
            vTaskDelay(pdMS_TO_TICKS(100));
            
            uint8_t deviceCount = sensors.getDeviceCount();
            if (deviceCount > 0) {
                deviceCount = std::min<uint8_t>(deviceCount, MAX_ONEWIRE_SENSORS);
                Logger::info("Found " + String(deviceCount) + " devices");
                
                scanSuccess = processFoundDevices(deviceCount, tempList);
                if (scanSuccess) break;
            }
            
            Logger::warning("Scan attempt " + String(retry + 1) + " failed");
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        
        if (scanSuccess) {
            updateSensorList(tempList);
            lastScanTime = millis();
        }
        
    } catch (const std::exception& e) {
        Logger::error("Exception during scan: " + String(e.what()));
        scanSuccess = false;
    }
    
    setBusBusy(false);
    return scanSuccess;
}

// Helper method to process devices found during scan
bool OneWireManager::processFoundDevices(uint8_t deviceCount, 
                                       std::vector<TemperatureSensor>& tempList) {
    bool anyDeviceProcessed = false;
    
    // Remove the MAX_ONEWIRE_SENSORS limit since we process the display sensor separately
    for (uint8_t i = 0; i < deviceCount; i++) {
        DeviceAddress tempAddr;
        if (sensors.getAddress(tempAddr, i)) {
            TemperatureSensor sensor = {};
            sensor.isActive = true;
            memcpy(sensor.address, tempAddr, sizeof(DeviceAddress));
            
            // Initialize sensor state
            sensor.valid = false;
            sensor.consecutiveErrors = 0;
            sensor.temperature = DEVICE_DISCONNECTED_C;
            sensor.lastValidReading = DEVICE_DISCONNECTED_C;
            sensor.lastReadTime = 0;
            
            if (sensors.validAddress(sensor.address)) {
                tempList.push_back(std::move(sensor));
                anyDeviceProcessed = true;
                Logger::debug("Added sensor: " + addressToString(tempAddr));
            }
        }
    }
    
    return anyDeviceProcessed;
}

// Update sensor list with thread safety and data preservation
void OneWireManager::updateSensorList(const std::vector<TemperatureSensor>& newList) {
    if (!verifyMutex()) return;
    
    if (xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        try {
            std::vector<TemperatureSensor> updatedList;
            updatedList.reserve(newList.size());
            
            // Preserve existing sensor data while updating the list
            for (const auto& newSensor : newList) {
                bool found = false;
                
                // Look for matching sensor in existing list
                for (const auto& existingSensor : sensorList) {
                    if (memcmp(existingSensor.address, newSensor.address, 8) == 0) {
                        // Preserve historical data for existing sensors
                        TemperatureSensor updated = newSensor;
                        if (existingSensor.valid) {
                            updated.temperature = existingSensor.temperature;
                            updated.lastValidReading = existingSensor.lastValidReading;
                            updated.lastReadTime = existingSensor.lastReadTime;
                            updated.valid = existingSensor.valid;
                            updated.consecutiveErrors = existingSensor.consecutiveErrors;
                        }
                        updatedList.push_back(updated);
                        found = true;
                        break;
                    }
                }
                
                // Add new sensors with initialized state
                if (!found) {
                    updatedList.push_back(newSensor);
                }
            }
            
            // Update the main sensor list
            sensorList = std::move(updatedList);
            Logger::info("Updated sensor list with " + String(sensorList.size()) + 
                        " sensors");
            
        } catch (const std::exception& e) {
            Logger::error("Exception during sensor list update: " + String(e.what()));
        }
        
        xSemaphoreGive(sensorMutex);
    } else {
        Logger::error("Failed to acquire mutex in updateSensorList");
    }
}

// Thread-safe access to the sensor list
const std::vector<TemperatureSensor>& OneWireManager::getSensorList() const {
    static const std::vector<TemperatureSensor> emptyList;
    
    if (!verifyMutex() || !sensorMutex) {
        Logger::error("Invalid mutex in getSensorList");
        return emptyList;
    }
    
    if (xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        Logger::error("Failed to acquire mutex in getSensorList");
        return emptyList;
    }
    
    const auto& list = sensorList;  
    xSemaphoreGive(sensorMutex);
    return list;
}

// Check if enough time has passed for a new scan
bool OneWireManager::shouldScan() const {
    return (millis() - lastScanTime) >= SCAN_INTERVAL;
}

bool OneWireManager::shouldRead() const {
    return (millis() - lastReadTime) >= READ_INTERVAL;
}

// Get the conversion status
bool OneWireManager::isConversionInProgress() const {
    return conversionInProgress;
}

// Thread-safe busy flag access
bool OneWireManager::isBusBusy() const {
    if (!verifyMutex()) return true;  // Assume busy if mutex invalid
    
    if (xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        bool busy = busyFlag;
        xSemaphoreGive(sensorMutex);
        return busy;
    }
    
    return true;  // Assume busy if mutex acquisition fails
}

// Private helper method to safely modify the busy flag
void OneWireManager::setBusBusy(bool busy) {
    if (!verifyMutex()) {
        Logger::error("Failed to verify mutex in setBusBusy");
        return;
    }
    
    if (xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        busyFlag = busy;
        xSemaphoreGive(sensorMutex);
        Logger::debug("Bus busy state changed to: " + String(busy));
    } else {
        Logger::error("Failed to acquire mutex in setBusBusy");
    }
}

// Verify mutex exists and is valid
bool OneWireManager::verifyMutex() const {
    if (!sensorMutex) {
        sensorMutex = xSemaphoreCreateMutex();
        if (!sensorMutex) {
            Logger::error("Failed to create mutex in verifyMutex");
            return false;
        }
        Logger::info("Created new mutex in verifyMutex");
    }
    return true;
}

String OneWireManager::addressToString(const uint8_t* address) const {
    if (!address) {
        return "Invalid Address";
    }
    
    char buffer[17];  // 8 bytes in hex (2 chars each) + null terminator
    snprintf(buffer, sizeof(buffer), "%02X%02X%02X%02X%02X%02X%02X%02X",
             address[0], address[1], address[2], address[3],
             address[4], address[5], address[6], address[7]);
    return String(buffer);
}

float OneWireManager::getCachedTemperature(const uint8_t* address) {
    if (!verifyMutex() || !sensorMutex) {
        Logger::error("Invalid mutex in getCachedTemperature");
        return DEVICE_DISCONNECTED_C;
    }
    
    float temp = DEVICE_DISCONNECTED_C;
    if (xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Log the address we're looking for
        String searchAddr = addressToString(address);
        Logger::debug("Searching for babel temperature for sensor: " + searchAddr);
        
        // Log all available sensors for debugging
        Logger::debug("Current sensor list:");
        for (const auto& sensor : sensorList) {
            String sensorAddr = addressToString(sensor.address);
            Logger::debug(" - " + sensorAddr + ": " + String(sensor.temperature, 2) + 
                         " (valid: " + String(sensor.valid) + ")");
        }
        
        for (const auto& sensor : sensorList) {
            if (memcmp(sensor.address, address, 8) == 0) {
                // Return last valid reading if recent, otherwise return current temp
                if (!sensor.valid && (millis() - sensor.lastReadTime) < 60000) {
                    temp = sensor.lastValidReading;
                    Logger::debug("Found sensor, using last valid reading: " + String(temp, 2));
                } else {
                    temp = sensor.temperature;
                    Logger::debug("Found sensor, using current temperature: " + String(temp, 2));
                }
                break;
            }
        }
        
        if (temp == DEVICE_DISCONNECTED_C) {
            Logger::debug("Sensor not found in list");
        }
        
        xSemaphoreGive(sensorMutex);
    } else {
        Logger::error("Failed to acquire mutex in getCachedTemperature");
    }
    
    return temp;
}

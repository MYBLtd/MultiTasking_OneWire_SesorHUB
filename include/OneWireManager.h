// OneWireManager.h
#pragma once

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <vector>
#include "SystemTypes.h"
#include "Config.h"

class OneWireManager {
public:
    explicit OneWireManager(uint8_t pin);
    
    // Temperature reading methods
    void startTemperatureConversion();
    bool checkAndCollectTemperatures();
    bool readTemperature(const uint8_t* address, float& temperature);
    
    // Device scanning methods
    bool scanDevices();
    void updateSensorList(const std::vector<TemperatureSensor>& newList);
    
    // Status check methods
    bool shouldScan() const;
    bool shouldRead() const;
    bool isConversionInProgress() const;
    bool isBusBusy() const;
    
    // Data access
    const std::vector<TemperatureSensor>& getSensorList() const;
    String addressToString(const uint8_t* address) const;

private:
    static constexpr int MAX_RETRIES = 3;
    
    OneWire oneWire;
    DallasTemperature sensors;
    std::vector<TemperatureSensor> sensorList;
    
    bool busyFlag;
    mutable SemaphoreHandle_t sensorMutex;
    
    // Timing control
    uint32_t lastScanTime;
    uint32_t lastReadTime;
    uint32_t conversionStartTime;
    bool conversionInProgress;
    
    // Private helper methods
    void setBusBusy(bool busy);
    bool verifyMutex() const;
    bool processFoundDevices(uint8_t deviceCount, std::vector<TemperatureSensor>& tempList);
};
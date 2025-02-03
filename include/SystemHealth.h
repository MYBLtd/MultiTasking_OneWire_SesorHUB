#pragma once
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class SystemHealth {
public:
    // Public methods
    static void init();
    static void update();
    static String getStatusReport();
    static void recordWatchdogNearMiss();
    
private:
    // Private methods
    static void updateHeapMetrics();
    static void updateStackMetrics();
    static void updateTaskMetrics();  // Add this declaration
    
    // Metrics structure to hold all system health data
    struct Metrics {
        uint32_t minHeapSeen;
        uint32_t watchdogNearMisses;
        uint32_t mqttReconnections;
        uint32_t httpOverflowCount;
        uint32_t oneWireErrors;
        uint32_t maxStackUsage1Wire;
        uint32_t maxStackUsageNetwork;
        uint32_t maxStackUsageControl;
    };
    
    // Static members
    static Metrics metrics;
    static SemaphoreHandle_t metricsMutex;
    static uint32_t lastUpdateTime;
};
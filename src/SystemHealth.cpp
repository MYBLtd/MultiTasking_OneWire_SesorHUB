// SystemHealth.cpp
#include "SystemHealth.h"
#include "Logger.h"
#include <esp_heap_caps.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

SystemHealth::Metrics SystemHealth::metrics;
SemaphoreHandle_t SystemHealth::metricsMutex = nullptr;
uint32_t SystemHealth::lastUpdateTime = 0;

void SystemHealth::init() {
    metricsMutex = xSemaphoreCreateMutex();
    metrics.minHeapSeen = ESP.getFreeHeap();
    metrics.watchdogNearMisses = 0;
    metrics.mqttReconnections = 0;
    metrics.httpOverflowCount = 0;
    metrics.oneWireErrors = 0;
    
    Logger::info("System Health monitoring initialized");
}

void SystemHealth::update() {
    if (!metricsMutex) return;
    
    uint32_t now = millis();
    if (now - lastUpdateTime < 1000) return;  // Only update once per second
    
    if (xSemaphoreTake(metricsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        updateHeapMetrics();
        updateStackMetrics();
        updateTaskMetrics();
        
        lastUpdateTime = now;
        xSemaphoreGive(metricsMutex);
    }
}

void SystemHealth::updateHeapMetrics() {
    size_t currentHeap = ESP.getFreeHeap();
    if (currentHeap < metrics.minHeapSeen) {
        metrics.minHeapSeen = currentHeap;
        Logger::warning("New minimum heap detected: " + String(currentHeap) + " bytes");
    }
}

void SystemHealth::updateStackMetrics() {
    // Get stack high water marks for key tasks using task handles
    TaskHandle_t oneWireHandle = xTaskGetHandle("OneWireTask");
    TaskHandle_t networkHandle = xTaskGetHandle("NetworkTask");
    TaskHandle_t controlHandle = xTaskGetHandle("ControlTask");
    
    if (oneWireHandle) {
        UBaseType_t stackMark = uxTaskGetStackHighWaterMark(oneWireHandle);
        metrics.maxStackUsage1Wire = stackMark;
        
        // Log warning if stack space is getting low
        if (stackMark < 512) {
            Logger::warning("Low stack in OneWireTask: " + String(stackMark) + " words remaining");
        }
    }
    
    if (networkHandle) {
        UBaseType_t stackMark = uxTaskGetStackHighWaterMark(networkHandle);
        metrics.maxStackUsageNetwork = stackMark;
        
        if (stackMark < 512) {
            Logger::warning("Low stack in NetworkTask: " + String(stackMark) + " words remaining");
        }
    }
    
    if (controlHandle) {
        UBaseType_t stackMark = uxTaskGetStackHighWaterMark(controlHandle);
        metrics.maxStackUsageControl = stackMark;
        
        if (stackMark < 512) {
            Logger::warning("Low stack in ControlTask: " + String(stackMark) + " words remaining");
        }
    }
}

void SystemHealth::updateTaskMetrics() {
    // Get overall task statistics using available FreeRTOS functions
    UBaseType_t taskCount = uxTaskGetNumberOfTasks();
    
    // Monitor for significant changes in task count
    static UBaseType_t lastTaskCount = 0;
    if (taskCount != lastTaskCount) {
        Logger::info("Task count changed: " + String(taskCount) + " tasks running");
        lastTaskCount = taskCount;
    }
    
    // Check system load using idle task statistics
    TaskHandle_t idleHandle = xTaskGetIdleTaskHandle();
    if (idleHandle) {
        UBaseType_t idleStack = uxTaskGetStackHighWaterMark(idleHandle);
        if (idleStack < 256) {  // Critical threshold for idle task
            Logger::error("Idle task stack critically low: " + String(idleStack) + " words");
        }
    }
}

String SystemHealth::getStatusReport() {
    String report;
    
    if (xSemaphoreTake(metricsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        report = "System Health Report\n"
                 "-------------------\n"
                 "Current Free Heap: " + String(ESP.getFreeHeap()) + " bytes\n"
                 "Minimum Heap Seen: " + String(metrics.minHeapSeen) + " bytes\n"
                 "Stack Usage (words remaining):\n"
                 "  OneWire Task: " + String(metrics.maxStackUsage1Wire) + "\n"
                 "  Network Task: " + String(metrics.maxStackUsageNetwork) + "\n"
                 "  Control Task: " + String(metrics.maxStackUsageControl) + "\n"
                 "Error Counts:\n"
                 "  Watchdog Near Misses: " + String(metrics.watchdogNearMisses) + "\n"
                 "  MQTT Reconnections: " + String(metrics.mqttReconnections) + "\n"
                 "  HTTP Overflows: " + String(metrics.httpOverflowCount) + "\n"
                 "  OneWire Errors: " + String(metrics.oneWireErrors);
        
        xSemaphoreGive(metricsMutex);
    }
    
    return report;
}

void SystemHealth::recordWatchdogNearMiss() {
    if (xSemaphoreTake(metricsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        metrics.watchdogNearMisses++;
        Logger::warning("Watchdog near-miss recorded - total: " + 
                       String(metrics.watchdogNearMisses));
        xSemaphoreGive(metricsMutex);
    }
}
// OneWireTask.cpp
#include "OneWireTask.h"
#include "Config.h"
#include "Logger.h"
#include "esp_task_wdt.h"

// Static member initialization
OneWireManager OneWireTask::manager(ONE_WIRE_BUS);
QueueHandle_t OneWireTask::commandQueue = nullptr;
SemaphoreHandle_t OneWireTask::dataMutex = nullptr;

void OneWireTask::init() {
    Logger::info("Initializing OneWire task");
    
    // Initialize watchdog
    ESP_ERROR_CHECK(esp_task_wdt_init(CONFIG_ESP_TASK_WDT_TIMEOUT_S, true));
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    
    // Create command queue and mutex
    commandQueue = xQueueCreate(10, sizeof(TaskMessage));
    dataMutex = xSemaphoreCreateMutex();
    
    if (!commandQueue || !dataMutex) {
        Logger::error("Failed to create OneWire task queues or mutex");
        return;
    }
    
    Logger::info("OneWire task initialized successfully");
}

void OneWireTask::start() {
    Logger::info("Starting OneWire task");
    
    xTaskCreate(
        taskFunction,
        "OneWireTask",
        ONEWIRE_TASK_STACK_SIZE,
        nullptr,
        ONEWIRE_TASK_PRIORITY,
        nullptr
    );
}

void OneWireTask::taskFunction(void* parameter) {
    Logger::info("OneWire task started");
    TickType_t lastWakeTime = xTaskGetTickCount();
    uint32_t lastScanTime = 0;
    uint32_t lastReadTime = 0;
    bool conversionStarted = false;
    
    // Initial scan
    Logger::info("Performing initial OneWire bus scan");
    if (manager.scanDevices()) {
        lastScanTime = millis();
        Logger::info("Initial scan completed successfully");
    }
    
    // Main task loop
    while (true) {
        esp_task_wdt_reset();
        
        // Process commands with higher priority
        TaskMessage msg;
        while (xQueueReceive(commandQueue, &msg, 0) == pdTRUE) {
            processCommand(msg);
        }
        
        // Current time for interval checks
        uint32_t currentTime = millis();
        
        // Periodic scan check
        if (currentTime - lastScanTime >= SCAN_INTERVAL) {
            if (!manager.isBusBusy() && !conversionStarted) {
                Logger::info("Starting periodic scan");
                if (manager.scanDevices()) {
                    lastScanTime = currentTime;
                }
            }
        }
        
        // Temperature reading state machine
        if (!conversionStarted) {
            if (currentTime - lastReadTime >= READ_INTERVAL) {
                if (!manager.isBusBusy()) {
                    manager.startTemperatureConversion();
                    conversionStarted = true;
                    Logger::debug("Started temperature conversion");
                }
            }
        } else {
            if (manager.checkAndCollectTemperatures()) {
                lastReadTime = currentTime;
                conversionStarted = false;
                Logger::debug("Temperature collection complete");
            }
        }
        
        // Fixed task interval
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(TASK_INTERVAL));
    }
}

void OneWireTask::processCommand(const TaskMessage& msg) {
    switch (msg.type) {
        case MessageType::SENSOR_SCAN_REQUEST:
            Logger::info("Processing scan request");
            if (!manager.isBusBusy()) {
                manager.scanDevices();
            } else {
                Logger::warning("Scan request ignored - bus busy");
            }
            break;
            
        case MessageType::TEMPERATURE_READ_REQUEST:
            Logger::info("Processing temperature read request");
            if (!manager.isBusBusy() && !manager.isConversionInProgress()) {
                manager.startTemperatureConversion();
            } else {
                Logger::warning("Read request ignored - operation in progress");
            }
            break;
            
        default:
            Logger::warning("Unknown command received");
            break;
    }
}
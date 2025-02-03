// OneWireTask.h
#pragma once

#include <Arduino.h>
#include "SystemTypes.h"
#include "OneWireManager.h"
#include "WebServer.h"
#include "Config.h"

class OneWireTask {
public:
    enum class MessageType {
        SENSOR_SCAN_REQUEST,
        TEMPERATURE_READ_REQUEST
    };
    
    struct TaskMessage {
        MessageType type;
    };
    
    static void init();
    static void start();
    
    // Make manager public for access by other tasks
    static OneWireManager manager;

private:
    static QueueHandle_t commandQueue;
    static SemaphoreHandle_t dataMutex;
    
    static void taskFunction(void* parameter);
    static void processCommand(const TaskMessage& msg);
};
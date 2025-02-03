// ControlTask.h
#pragma once

#include <Arduino.h>
#include "DisplayManager.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include "SystemTypes.h"
#include "Config.h"
#include "Logger.h"

class ControlTask {
public:
    static void init();
    static void start();
    static void updateRelayRequest(uint8_t relayId, bool state);
    static void updateDisplayValue(float temperature);
    static bool getRelayState(uint8_t relayId);
    
private:
    static void taskFunction(void* parameter);
    static String addressToString(const uint8_t* address);
    
    static DisplayManager display;
    static RelayState relayStates[2];
    static QueueHandle_t controlQueue;
    static SemaphoreHandle_t stateMutex;
};
// WebServer.h
#pragma once

#include <ESPAsyncWebServer.h>
#include "OneWireManager.h"
#include "PreferencesApiHandler.h"
#include "Logger.h"
#include "SharedDefinitions.h"
#include "SystemTypes.h"  // For TemperatureSensor definition

class WebServer {
public:
    WebServer(OneWireManager& owManager);
    void begin();

private:
    AsyncWebServer server;
    OneWireManager& oneWireManager;
    PreferencesApiHandler preferencesHandler;

    void setupRoutes();
    void setupCorsHeaders();
    void setupStaticFiles();
    void setupApiRoutes();
    
    void handleSensorsRequest(AsyncWebServerRequest* request);
    void handleOptionsRequest(AsyncWebServerRequest* request);
    
    static String addressToString(const uint8_t* address);
    static void stringToAddress(const char* str, uint8_t* address);
    JsonObject createSensorJson(JsonArray& array, const TemperatureSensor& sensor);

    void sendErrorResponse(AsyncWebServerRequest* request, int code, const String& message);
    void sendJsonResponse(AsyncWebServerRequest* request, const String& json);
};

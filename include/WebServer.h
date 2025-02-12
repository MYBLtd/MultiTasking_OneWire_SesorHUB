#pragma once

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "OneWireManager.h"
#include "PreferencesApiHandler.h"
#include "Logger.h"
#include "SharedDefinitions.h"
#include "SystemTypes.h"
#include "AuthManager.h"  // Add this include

class WebServer {
public:
    WebServer(OneWireManager& owManager);
    void begin();

private:
    AsyncWebServer server;
    OneWireManager& oneWireManager;
    PreferencesApiHandler preferencesHandler;

    // Setup methods
    void setupRoutes();
    void setupCorsHeaders();
    void setupStaticFiles();
   
    // Request handlers
    void handleSensorsRequest(AsyncWebServerRequest* request);
    void handleOptionsRequest(AsyncWebServerRequest* request);
    void handleLoginRequest(AsyncWebServerRequest* request, JsonVariant& json);
    void handleLogoutRequest(AsyncWebServerRequest* request);

    // Authentication helpers
    bool isAuthenticatedRequest(AsyncWebServerRequest* request);
    static String extractToken(AsyncWebServerRequest* request);

    // Helper methods
    JsonObject createSensorJson(JsonArray& array, const TemperatureSensor& sensor);
    void sendErrorResponse(AsyncWebServerRequest* request, int code, const String& message);
    void sendJsonResponse(AsyncWebServerRequest* request, const String& json);
    static String addressToString(const uint8_t* address);
    static void stringToAddress(const char* str, uint8_t* address);
};
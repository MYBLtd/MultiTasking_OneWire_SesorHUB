// WebServer.cpp
#include "WebServer.h"
#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <SPIFFS.h>
#include <map>  // Added for std::map
#include "DallasTemperature.h"  // For DEVICE_DISCONNECTED_C

// Rate limiting implementation using a simple circular buffer
// This is more memory-efficient than using a map
class RateLimiter {
private:
    static const size_t MAX_CLIENTS = 10;  // Adjust based on memory constraints
    struct ClientEntry {
        uint32_t ip;
        unsigned long lastRequest;
    };
    ClientEntry clients[MAX_CLIENTS];
    size_t currentIndex;
    
public:
    RateLimiter() : currentIndex(0) {
        memset(clients, 0, sizeof(clients));
    }
    
    bool shouldLimit(uint32_t ipAsInt, unsigned long now, unsigned long interval) {
        // Look for existing client
        for (size_t i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].ip == ipAsInt) {
                if (now - clients[i].lastRequest < interval) {
                    return true;  // Should limit
                }
                clients[i].lastRequest = now;
                return false;
            }
        }
        
        // Add new client to circular buffer
        clients[currentIndex].ip = ipAsInt;
        clients[currentIndex].lastRequest = now;
        currentIndex = (currentIndex + 1) % MAX_CLIENTS;
        return false;
    }
};

// Static rate limiter instance
static RateLimiter rateLimiter;
WebServer::WebServer(OneWireManager& owManager) 
    : server(80)
    , oneWireManager(owManager)
    , preferencesHandler(owManager) {
}

void WebServer::begin() {
    Logger::info("Initializing web server...");
    Logger::info("Files in SPIFFS:");
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while(file) {
        Logger::info(" - " + String(file.name()) + " (" + String(file.size()) + " bytes)");
        file = root.openNextFile();
    }

    setupRoutes();
    server.begin();
    Logger::info("Web server started successfully");
}

void WebServer::setupRoutes() {
    setupCorsHeaders();
    
    // Request body handler with size limiting and rate limiting
    server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        // Size limiting
        static const size_t MAX_REQUEST_SIZE = 4096;
        if (total > MAX_REQUEST_SIZE) {
            request->send(413, "text/plain", "Request entity too large");
            return;
        }

        // Connection status check
        if (!request->client()->canSend()) {
            request->send(503, "text/plain", "Service unavailable");
            return;
        }

        // Rate limiting
        IPAddress clientIP = request->client()->remoteIP();
        uint32_t ipAsInt = (uint32_t)clientIP;
        
        if (rateLimiter.shouldLimit(ipAsInt, millis(), 1000)) {
            request->send(429, "text/plain", "Too many requests");
            return;
        }
    });

    setupStaticFiles();
    setupApiRoutes();
}


void WebServer::setupCorsHeaders() {
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");
    DefaultHeaders::Instance().addHeader("Access-Control-Max-Age", "86400");
}

void WebServer::setupStaticFiles() {
    AsyncStaticWebHandler* handler = &server.serveStatic("/", SPIFFS, "/")
        .setDefaultFile("index.html")
        .setCacheControl("no-store");

    server.onNotFound([](AsyncWebServerRequest *request){
        Logger::info("404 Not Found: " + request->url());
        request->send(404);
    });
}

void WebServer::setupApiRoutes() {
    // Sensors endpoint
    server.on("/api/sensors", HTTP_GET, 
        [this](AsyncWebServerRequest *request) { 
            handleSensorsRequest(request); 
        }
    );

    // Aux display endpoint
    server.on("/api/aux_display", HTTP_GET, 
        [this](AsyncWebServerRequest *request) { 
            handleAuxDisplayRequest(request); 
        }
    );

    // Preferences endpoints
    server.on("/api/preferences", HTTP_GET, 
        [this](AsyncWebServerRequest *request) {
            try {
                String jsonResponse = this->preferencesHandler.handleGet();
                sendJsonResponse(request, jsonResponse);
            } catch (const std::exception& e) {
                Logger::error("Exception in preferences GET API: " + String(e.what()));
                sendErrorResponse(request, 500, "Internal server error");
            }
        }
    );

    // POST handler for preferences
    AsyncCallbackJsonWebHandler* preferencesHandler = new AsyncCallbackJsonWebHandler(
        "/api/preferences",
        [this](AsyncWebServerRequest *request, JsonVariant &json) {
            try {
                String jsonStr;
                serializeJson(json, jsonStr);
                Logger::debug("Received preferences update: " + jsonStr);
                
                bool success = this->preferencesHandler.handlePost(jsonStr);
                if (success) {
                    sendJsonResponse(request, "{\"status\":\"success\"}");
                } else {
                    sendErrorResponse(request, 400, "Invalid preferences data");
                }
            } catch (const std::exception& e) {
                Logger::error("Exception in preferences POST API: " + String(e.what()));
                sendErrorResponse(request, 500, "Internal server error");
            }
        }
    );
    
    preferencesHandler->setMaxContentLength(1024);
    server.addHandler(preferencesHandler);

    // Handle CORS preflight requests
    server.on("/api/preferences", HTTP_OPTIONS, 
        [this](AsyncWebServerRequest *request) { 
            handleOptionsRequest(request); 
        }
    );
}

void WebServer::handleSensorsRequest(AsyncWebServerRequest *request) {
    try {
        const auto& sensorList = oneWireManager.getSensorList();
        AsyncJsonResponse *response = new AsyncJsonResponse(false, 4096);
        JsonArray array = response->getRoot().to<JsonArray>();
        
        Logger::debug("Processing " + String(sensorList.size()) + " sensors for response");
        
        for(const auto& sensor : sensorList) {
            createSensorJson(array, sensor);
        }
        
        response->setLength();
        
        String jsonStr;
        serializeJsonPretty(response->getRoot(), jsonStr);
        Logger::debug("Sensors API response: " + jsonStr);
        
        request->send(response);
        
    } catch (const std::exception& e) {
        Logger::error("Exception in sensor API: " + String(e.what()));
        sendErrorResponse(request, 500, "Internal server error");
    }
}

JsonObject WebServer::createSensorJson(JsonArray& array, const TemperatureSensor& sensor) {
    JsonObject obj = array.createNestedObject();
    
    // Access the address directly from the sensor struct
    String addr = addressToString(sensor.address);
    obj["address"] = addr;
    
    // Get the sensor name using the address
    String name = PreferencesManager::getSensorName(sensor.address);
    if (name.length() > 0) {
        obj["name"] = name;
    }
    
    // Access temperature data directly from the sensor struct
    obj["temperature"] = sensor.valid ? sensor.temperature : DEVICE_DISCONNECTED_C;
    obj["valid"] = sensor.valid;
    obj["lastReadTime"] = sensor.lastReadTime;
    
    Logger::debug("Added sensor: " + addr + 
                 (name.length() > 0 ? " (" + name + ")" : "") +
                 ", temp: " + String(sensor.temperature, 2) + 
                 ", valid: " + String(sensor.valid) + 
                 ", time: " + String(sensor.lastReadTime));
                 
    return obj;
}

void WebServer::handleOptionsRequest(AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(204);
    request->send(response);
}

void WebServer::sendErrorResponse(AsyncWebServerRequest* request, int code, const String& message) {
    String errorJson = "{\"error\":\"" + message + "\"}";
    request->send(code, "application/json", errorJson);
}

void WebServer::sendJsonResponse(AsyncWebServerRequest* request, const String& json) {
    request->send(200, "application/json", json);
}

String WebServer::addressToString(const uint8_t* address) {
    char buffer[17];
    snprintf(buffer, sizeof(buffer), "%02X%02X%02X%02X%02X%02X%02X%02X",
             address[0], address[1], address[2], address[3],
             address[4], address[5], address[6], address[7]);
    return String(buffer);
}

void WebServer::stringToAddress(const char* str, uint8_t* address) {
    for (int i = 0; i < 8; i++) {
        char byte[3] = {str[i*2], str[i*2 + 1], '\0'};
        address[i] = strtol(byte, nullptr, 16);
    }
}

void WebServer::handleAuxDisplayRequest(AsyncWebServerRequest *request) {
    AsyncJsonResponse *response = new AsyncJsonResponse(false, 128);
    JsonObject root = response->getRoot();
    
    root["temperature"] = lastAuxDisplayTemp;
    root["timestamp"] = millis();

    response->setLength();
    request->send(response);
}

void WebServer::updateAuxDisplayTemp(float temp) {
    lastAuxDisplayTemp = temp;
}

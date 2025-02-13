// WebServer.cpp
#include "WebServer.h"
#include "AuthManager.h"
#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <SPIFFS.h>
#include "DallasTemperature.h"  // For DEVICE_DISCONNECTED_C
#include <map>
#define DEBUG
// Rate limiting implementation using a circular buffer for memory efficiency
class RateLimiter {
private:
    static const size_t MAX_CLIENTS = 10;
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
        
        // Add new client using circular buffer
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
    
    // List all files in SPIFFS for debugging
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
    // Setup CORS headers first (keeping your existing CORS setup)
    setupCorsHeaders();

    // Set up authentication routes first for login/logout
    AsyncCallbackJsonWebHandler* loginHandler = new AsyncCallbackJsonWebHandler(
        "/api/login",
        [this](AsyncWebServerRequest* request, JsonVariant& json) {
            handleLoginRequest(request, json);
        }
    );
    loginHandler->setMaxContentLength(1024);
    server.addHandler(loginHandler);

    server.on("/api/logout", HTTP_POST, 
        [this](AsyncWebServerRequest* request) {
            handleLogoutRequest(request);
        });

    // Set up protected API routes with direct authentication checks
    server.on("/api/sensors", HTTP_GET, 
        [this](AsyncWebServerRequest* request) {
            Logger::debug("Handling /api/sensors request");
            if (!isAuthenticatedRequest(request)) {
                Logger::warning("Unauthorized sensors request");
                request->send(401);
                return;
            }
            handleSensorsRequest(request);
        });

    server.on("/api/relay", HTTP_GET, 
        [this](AsyncWebServerRequest* request) {
            Logger::debug("Handling /api/relay GET request");
            if (!isAuthenticatedRequest(request)) {
                Logger::warning("Unauthorized relay status request");
                request->send(401);
                return;
            }
            handleRelayRequest(request);
        });

    AsyncCallbackJsonWebHandler* relayHandler = new AsyncCallbackJsonWebHandler(
        "/api/relay",
        [this](AsyncWebServerRequest* request, JsonVariant& json) {
            Logger::debug("Handling /api/relay POST request");
            if (!isAuthenticatedRequest(request)) {
                Logger::warning("Unauthorized relay control request");
                request->send(401);
                return;
            }
            handleRelayControlRequest(request, json);
        }
    );
    relayHandler->setMaxContentLength(1024);
    server.addHandler(relayHandler);

    server.on("/api/preferences", HTTP_GET,
        [this](AsyncWebServerRequest* request) {
            Logger::debug("Handling /api/preferences GET request");
            if (!isAuthenticatedRequest(request)) {
                Logger::warning("Unauthorized preferences request");
                request->send(401);
                return;
            }
            try {
                String jsonResponse = preferencesHandler.handleGet();
                Logger::debug("Preferences response: " + jsonResponse);
                sendJsonResponse(request, jsonResponse);
            } catch (const std::exception& e) {
                Logger::error("Exception in preferences GET: " + String(e.what()));
                sendErrorResponse(request, 500, "Internal server error");
            }
        });

    // Add the preferences POST handler
    AsyncCallbackJsonWebHandler* preferencesHandler = new AsyncCallbackJsonWebHandler(
        "/api/preferences",
        [this](AsyncWebServerRequest* request, JsonVariant& json) {
            Logger::debug("Handling /api/preferences POST request");
            if (!isAuthenticatedRequest(request)) {
                Logger::warning("Unauthorized preferences POST request");
                request->send(401);
                return;
            }
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
                Logger::error("Exception in preferences POST: " + String(e.what()));
                sendErrorResponse(request, 500, "Internal server error");
            }
        }
    );
    preferencesHandler->setMaxContentLength(1024);
    server.addHandler(preferencesHandler);

    // Set up static file handling
    server.on("/login", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/login.html", "text/html");
    });

    // Handle all other static files and default routes
    server.on("/*", HTTP_GET, [this](AsyncWebServerRequest *request) {
        String path = request->url();
        Logger::debug("Handling static request: " + path);
        
        // Allow direct access to login page
        if (path == "/login" || path == "/login.html") {
            request->send(SPIFFS, "/login.html", "text/html");
            return;
        }
        
        // Check authentication for all other pages
        if (!isAuthenticatedRequest(request)) {
            Logger::warning("Unauthorized access attempt to: " + path);
            request->redirect("/login");
            return;
        }
        
        // Serve authenticated requests
        if (path == "/" || path == "/index.html") {
            request->send(SPIFFS, "/index.html", "text/html");
        } else if (SPIFFS.exists(path)) {
            request->send(SPIFFS, path);
        } else {
            Logger::warning("File not found: " + path);
            request->send(404);
        }
    });
}

void WebServer::setupCorsHeaders() {
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", 
        "Content-Type, Authorization");
    DefaultHeaders::Instance().addHeader("Access-Control-Max-Age", "86400");
}


void WebServer::setupStaticFiles() {
    // Login page - unprotected
    server.on("/login", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/login.html", "text/html");
    });
    
    // Protected static file handler
    server.on("/*", HTTP_GET, [this](AsyncWebServerRequest *request) {
        String path = request->url();
        
        // Allow access to login page without authentication
        if (path == "/login" || path == "/login.html") {
            request->send(SPIFFS, "/login.html", "text/html");
            return;
        }
        
        // Check authentication for all other pages
        if (!isAuthenticatedRequest(request)) {
            // Redirect to login page
            request->redirect("/login");
            return;
        }
        
        // If authenticated, serve the requested file
        if (SPIFFS.exists(path)) {
            request->send(SPIFFS, path);
        } else if (path == "/" || path == "/index.html") {
            request->send(SPIFFS, "/index.html", "text/html");
        } else {
            request->send(404);
        }
    });
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
        request->send(response);
        
    } catch (const std::exception& e) {
        Logger::error("Exception in sensor API: " + String(e.what()));
        sendErrorResponse(request, 500, "Internal server error");
    }
}

JsonObject WebServer::createSensorJson(JsonArray& array, const TemperatureSensor& sensor) {
    JsonObject obj = array.createNestedObject();
    
    String addr = addressToString(sensor.address);
    obj["address"] = addr;
    
    String name = PreferencesManager::getSensorName(sensor.address);
    if (name.length() > 0) {
        obj["name"] = name;
    }
    
    obj["temperature"] = sensor.valid ? sensor.temperature : DEVICE_DISCONNECTED_C;
    obj["valid"] = sensor.valid;
    obj["lastReadTime"] = sensor.lastReadTime;
    
    // Check if this sensor is the currently selected BabelSensor
    uint8_t displaySensorAddr[8];
    PreferencesManager::getDisplaySensor(displaySensorAddr);
    if (memcmp(sensor.address, displaySensorAddr, 8) == 0) {
        obj["isBabelSensor"] = true;
        obj["babelTemperature"] = sensor.temperature;  // Add this alias for compatibility
    }
    
    Logger::debug("Added sensor: " + addr + 
                 (name.length() > 0 ? " (" + name + ")" : "") +
                 ", temp: " + String(sensor.temperature, 2) + 
                 ", valid: " + String(sensor.valid) +
                 ", babel: " + String(memcmp(sensor.address, displaySensorAddr, 8) == 0));
                 
    return obj;
}

void WebServer::handleOptionsRequest(AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(204);
    request->send(response);
}

void WebServer::handleLoginRequest(AsyncWebServerRequest* request, JsonVariant& json) {
    JsonObject jsonObj = json.as<JsonObject>();
    if (!jsonObj.containsKey("username") || !jsonObj.containsKey("password")) {
        request->send(400, "application/json", "{\"error\":\"Missing credentials\"}");
        return;
    }

    String username = jsonObj["username"].as<String>();
    String password = jsonObj["password"].as<String>();

    if (AuthManager::validateCredentials(username, password)) {
        String token = AuthManager::createSession(username);
        AsyncWebServerResponse* response = request->beginResponse(200, "application/json", 
            "{\"token\":\"" + token + "\"}");
        response->addHeader("Set-Cookie", 
            "session=" + token + "; Path=/; SameSite=Strict; HttpOnly");
        request->send(response);
        Logger::info("Login successful for user: " + username);
    } else {
        Logger::warning("Failed login attempt for user: " + username);
        request->send(401, "application/json", "{\"error\":\"Invalid credentials\"}");
    }
}

void WebServer::handleLogoutRequest(AsyncWebServerRequest* request) {
    String token = extractToken(request);
    if (!token.isEmpty()) {
        AuthManager::revokeSession(token);
    }
    
    AsyncWebServerResponse* response = request->beginResponse(200, "application/json", 
        "{\"status\":\"logged out\"}");
    
    // Clear the session cookie
    response->addHeader("Set-Cookie", 
        "session=; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT");
    
    request->send(response);
    Logger::info("User logged out successfully");
}

bool WebServer::isAuthenticatedRequest(AsyncWebServerRequest* request) {
    String token = extractToken(request);
    Logger::debug("Checking auth token: " + (token.isEmpty() ? "empty" : token));
    
    if (token.isEmpty()) {
        Logger::warning("No auth token found");
        return false;
    }
    
    bool valid = AuthManager::validateSession(token);
    Logger::debug("Token validation result: " + String(valid ? "valid" : "invalid"));
    return valid;
}

String WebServer::extractToken(AsyncWebServerRequest* request) {
    String token;
    
    // Check Authorization header first
    if (request->hasHeader("Authorization")) {
        String auth = request->header("Authorization");
        Logger::debug("Found Authorization header: " + auth);
        if (auth.startsWith("Bearer ")) {
            token = auth.substring(7);
        }
    }
    
    // Check cookie if no Authorization header token
    if (token.isEmpty() && request->hasHeader("Cookie")) {
        String cookies = request->header("Cookie");
        Logger::debug("Found Cookie header: " + cookies);
        int tokenStart = cookies.indexOf("session=");
        if (tokenStart >= 0) {
            tokenStart += 8;  // Length of "session="
            int tokenEnd = cookies.indexOf(";", tokenStart);
            if (tokenEnd < 0) tokenEnd = cookies.length();
            token = cookies.substring(tokenStart, tokenEnd);
        }
    }
    
    return token;
}

void WebServer::sendErrorResponse(AsyncWebServerRequest* request, int code, 
                                const String& message) {
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

void WebServer::handleRelayRequest(AsyncWebServerRequest* request) {
    try {
        AsyncJsonResponse* response = new AsyncJsonResponse(false, 1024);
        JsonArray array = response->getRoot().to<JsonArray>();
        
        for (int i = 0; i < 2; i++) {
            JsonObject relay = array.createNestedObject();
            relay["relay_id"] = i;
            relay["state"] = ControlTask::getRelayState(i);
            
            String name = PreferencesManager::getRelayName(i);
            if (name.length() > 0) {
                relay["name"] = name;
            }
        }
        
        response->setLength();
        request->send(response);
        
    } catch (const std::exception& e) {
        Logger::error("Exception in relay status API: " + String(e.what()));
        sendErrorResponse(request, 500, "Internal server error");
    }
}

void WebServer::handleRelayControlRequest(AsyncWebServerRequest* request, JsonVariant& json) {
    try {
        JsonObject jsonObj = json.as<JsonObject>();
        
        if (!jsonObj.containsKey("relay_id") || !jsonObj.containsKey("state")) {
            sendErrorResponse(request, 400, "Missing relay_id or state");
            return;
        }
        
        int relayId = jsonObj["relay_id"].as<int>();
        bool state = jsonObj["state"].as<bool>();
        
        if (relayId < 0 || relayId > 1) {
            sendErrorResponse(request, 400, "Invalid relay_id");
            return;
        }
        
        ControlTask::updateRelayRequest(relayId, state);
        sendJsonResponse(request, "{\"status\":\"success\"}");
        
    } catch (const std::exception& e) {
        Logger::error("Exception in relay control API: " + String(e.what()));
        sendErrorResponse(request, 500, "Internal server error");
    }
}

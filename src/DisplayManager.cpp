// DisplayManager.cpp
#include "DisplayManager.h"
#include "Logger.h"

DisplayManager::DisplayManager(uint8_t clkPin, uint8_t dioPin)
    : display(clkPin, dioPin)
    , currentTemp(0.0f) {
}

void DisplayManager::init() {
    Logger::info("Initializing TM1637 display");
    display.begin();
    display.setBrightnessPercent(90);
    
    showMessage("TEST");
    delay(2000);
    
    showMessage("----");
    Logger::info("Display initialization complete");
}

void DisplayManager::update() {
    char tempStr[5];  // Buffer for temperature string
    
    if (currentTemp < -9.9 || currentTemp > 99.9) {
        showMessage("ERR");
        return;
    }
    
    // Format temperature with one decimal place
    int temp = abs(round(currentTemp * 10));
    int whole = temp / 10;
    int decimal = temp % 10;
    
    if (currentTemp < 0) {
        snprintf(tempStr, sizeof(tempStr), "-%d.%d", whole, decimal);
    } else {
        snprintf(tempStr, sizeof(tempStr), "%d.%d", whole, decimal);
    }
    
    showMessage(tempStr);
    Logger::info("Display update: " + String(tempStr));
}

void DisplayManager::showMessage(const char* text) {
    display.display(text, false, false, 0);
    Logger::info("Display message: " + String(text));
}

void DisplayManager::setTemperature(float temp) {
    if (temp != currentTemp) {
        currentTemp = temp;
        update();
    }
}

void DisplayManager::setBrightness(uint8_t percent) {
    display.setBrightnessPercent(percent);
}

void DisplayManager::clear() {
    showMessage("    ");  // Four spaces
}
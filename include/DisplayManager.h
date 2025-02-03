// DisplayManager.h
#pragma once
#include <Arduino.h>
#include <TM1637.h>

class DisplayManager {
public:
    DisplayManager(uint8_t clkPin, uint8_t dioPin);
    void init();
    void update();
    void setTemperature(float temp);
    void setBrightness(uint8_t percent);
    void clear();
    void showMessage(const char* text);  // New method for text display
    
private:
    TM1637 display;      // The TM1637 driver instance
    float currentTemp;   // Current temperature value
};
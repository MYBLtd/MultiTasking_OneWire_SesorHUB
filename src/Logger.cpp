// Logger.cpp
#include "Logger.h"

// Initialize static members
Logger::Level Logger::currentLevel = Logger::Level::INFO;
uint8_t Logger::enabledCategories = 0xFF;  // All categories enabled by default
unsigned long Logger::lastMemoryLog = 0;

void Logger::setLogLevel(Level level) {
    currentLevel = level;
}

void Logger::enableCategory(Category category) {
    enabledCategories |= (1 << static_cast<uint8_t>(category));
}

void Logger::disableCategory(Category category) {
    enabledCategories &= ~(1 << static_cast<uint8_t>(category));
}

void Logger::error(const String& message, Category category) {
    logMessage(Level::ERROR, category, message);
}

void Logger::warning(const String& message, Category category) {
    logMessage(Level::WARNING, category, message);
}

void Logger::info(const String& message, Category category) {
    logMessage(Level::INFO, category, message);
}

void Logger::debug(const String& message, Category category) {
    logMessage(Level::DEBUG, category, message);
}

void Logger::trace(const String& message, Category category) {
    logMessage(Level::TRACE, category, message);
}

const char* Logger::getLevelString(Level level) {
    switch (level) {
        case Level::ERROR:   return "ERROR";
        case Level::WARNING: return "WARN ";
        case Level::INFO:    return "INFO ";
        case Level::DEBUG:   return "DEBUG";
        case Level::TRACE:   return "TRACE";
        default:            return "?????";
    }
}

const char* Logger::getCategoryString(Category category) {
    switch (category) {
        case Category::SYSTEM:  return "SYS";
        case Category::NETWORK: return "NET";
        case Category::SENSORS: return "SNR";
        case Category::MEMORY:  return "MEM";
        case Category::GENERAL: return "GEN";
        default:               return "???";
    }
}

bool Logger::isCategoryEnabled(Category category) {
    return (enabledCategories & (1 << static_cast<uint8_t>(category))) != 0;
}

void Logger::logMessage(Level level, Category category, const String& message) {
    // Check if this message should be logged based on level and category
    if (static_cast<int>(level) > static_cast<int>(currentLevel) || 
        !isCategoryEnabled(category)) {
        return;
    }
    
    // For memory category, implement rate limiting
    if (category == Category::MEMORY) {
        unsigned long now = millis();
        if (now - lastMemoryLog < MEMORY_LOG_INTERVAL) {
            return;
        }
        lastMemoryLog = now;
    }

    // Get current timestamp
    unsigned long timestamp = millis();
    
    // Format and output the log message
    char timeStr[10];
    snprintf(timeStr, sizeof(timeStr), "[%6lu]", timestamp);
    
    Serial.printf("%s[%s][%s] %s\n", 
                 timeStr,
                 getLevelString(level),
                 getCategoryString(category),
                 message.c_str());
}
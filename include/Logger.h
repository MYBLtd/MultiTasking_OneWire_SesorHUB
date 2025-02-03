// Logger.h
#pragma once

#include <Arduino.h>

class Logger {
public:
    // Log levels in order of increasing verbosity
    enum class Level {
        ERROR = 0,    // Critical errors that prevent normal operation
        WARNING = 1,  // Important issues that don't stop operation
        INFO = 2,     // Normal operational messages
        DEBUG = 3,    // Detailed information for troubleshooting
        TRACE = 4     // Very detailed program flow information
    };
    
    // Log categories to organize different types of messages
    enum class Category {
        SYSTEM,    // Core system events (startup, shutdown, etc)
        NETWORK,   // Network and MQTT related events
        SENSORS,   // Temperature sensor operations
        MEMORY,    // Memory and resource usage
        GENERAL    // Uncategorized messages
    };

    // Static methods to set logging configuration
    static void setLogLevel(Level level);
    static void enableCategory(Category category);
    static void disableCategory(Category category);
    
    // Core logging methods
    static void error(const String& message, Category category = Category::GENERAL);
    static void warning(const String& message, Category category = Category::GENERAL);
    static void info(const String& message, Category category = Category::GENERAL);
    static void debug(const String& message, Category category = Category::GENERAL);
    static void trace(const String& message, Category category = Category::GENERAL);
    
private:
    static Level currentLevel;                // Current logging level
    static uint8_t enabledCategories;         // Bitfield of enabled categories
    static unsigned long lastMemoryLog;       // Timestamp of last memory log
    static constexpr unsigned int MEMORY_LOG_INTERVAL = 5000;  // 5 seconds between memory logs
    
    // Internal helper methods
    static void logMessage(Level level, Category category, const String& message);
    static const char* getLevelString(Level level);
    static const char* getCategoryString(Category category);
    static bool isCategoryEnabled(Category category);
};
#pragma once
#include <Arduino.h>

// Base class for preference storage implementations
class PreferenceStorage {
public:
    virtual bool begin(const char* name, bool readOnly) = 0;
    virtual bool putString(const char* key, const char* value) = 0;
    virtual String getString(const char* key, const char* defaultValue) = 0;
    virtual bool putUInt(const char* key, uint32_t value) = 0;
    virtual uint32_t getUInt(const char* key, uint32_t defaultValue) = 0;
    virtual bool remove(const char* key) = 0;  // Add this line
    virtual ~PreferenceStorage() = default;
};
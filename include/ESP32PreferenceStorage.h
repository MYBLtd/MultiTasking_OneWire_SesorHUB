#pragma once

#include "PreferenceStorage.h"
#include <Preferences.h>

class ESP32PreferenceStorage : public PreferenceStorage {
private:
    Preferences prefs;

public:
    bool begin(const char* name, bool readOnly) override {
        return prefs.begin(name, readOnly);
    }
    
    bool putString(const char* key, const char* value) override {
        return prefs.putString(key, value);
    }
    
    String getString(const char* key, const char* defaultValue) override {
        return prefs.getString(key, defaultValue);
    }
    
    bool putUInt(const char* key, uint32_t value) override {
        return prefs.putUInt(key, value);
    }
    
    uint32_t getUInt(const char* key, uint32_t defaultValue) override {
        return prefs.getUInt(key, defaultValue);
    }
    
    ~ESP32PreferenceStorage() {
        prefs.end();
    }

        bool remove(const char* key) override {
        return prefs.remove(key);
    }
};
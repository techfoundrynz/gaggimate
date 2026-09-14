#pragma once
#include "Arduino.h"
#include <map>
class Preferences {
  public:
    inline static std::map<std::string, std::string> values;
    inline static bool failWrites = false;
    bool begin(const char *, bool) { return true; }
    bool getBool(const char *key, bool fallback) { return values.count(key) ? values[key] == "1" : fallback; }
    String getString(const char *key, const char *fallback) { return values.count(key) ? values[key] : fallback; }
    size_t putBool(const char *key, bool value) { return putString(key, value ? "1" : "0"); }
    size_t putString(const char *key, const char *value) {
        if (failWrites) return 0;
        values[key] = value;
        return values[key].size();
    }
};

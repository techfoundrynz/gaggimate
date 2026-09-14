#pragma once
#include <functional>
#include <map>
#include <string>
struct Event {};
class PluginManager {
  public:
    std::map<std::string, std::function<void(const Event &)>> listeners;
    void on(const char *event, std::function<void(const Event &)> callback) { listeners[event] = callback; }
    void trigger(const char *event) { listeners.at(event)(Event{}); }
};

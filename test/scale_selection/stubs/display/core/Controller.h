#pragma once
#define CONTROLLER_H
#include <display/core/ScaleStatus.h>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

unsigned long millis();
#define ESP_LOGW(...) ((void)0)
constexpr int MODE_BREW = 1, MODE_GRIND = 2, MODE_WATER = 3;
enum class VolumetricMeasurementSource { SCALE, FLOW_ESTIMATION };
enum class ProcessTarget { VOLUMETRIC, TIME };
struct Event {};
struct PluginManager {
    std::map<std::string, std::function<void(const Event &)>> events;
    void on(const char *name, std::function<void(const Event &)> callback) { events[name] = callback; }
    void trigger(const char *name) { events.at(name)(Event{}); }
};
struct Process {
    int type;
    explicit Process(int value) : type(value) {}
    int getType() const { return type; }
};
struct BrewProcess : Process {
    BrewProcess() : Process(MODE_BREW) {}
    ProcessTarget target = ProcessTarget::VOLUMETRIC;
};
struct GrindProcess : Process {
    GrindProcess() : Process(MODE_GRIND) {}
    ProcessTarget target = ProcessTarget::VOLUMETRIC;
};
struct Settings {
    bool wired = false;
    std::string saved = "test-scale";
    bool isHardwareScaleActive() const { return wired; }
    std::string getSavedScale() const { return saved; }
};
struct Client { int tares = 0; void tare() { ++tares; } };
class Controller {
  public:
    void setupScale();
    gm::ScaleStatus getScaleStatus() const;
    bool isScaleSelected(ScaleSource source) const { return source == scaleSource; }
    bool onScaleMeasurement(ScaleSource source, double value);
    void onScaleDisconnected(ScaleSource source);
    void tareSelectedScale();
    void tareScale();
    bool isScaleHealthy() const;
    void onScaleUnavailable();
    void onVolumetricMeasurement(double value, VolumetricMeasurementSource) { weights.push_back(value); }
    void deactivateLocked(std::vector<const char *> &events) { ++stops; events.push_back("stopped"); }
    void dispatchEvents(const std::vector<const char *> &events) { dispatched += events.size(); }
    Settings settings;
    PluginManager manager;
    PluginManager *pluginManager = &manager;
    Client comms;
    ScaleSource scaleSource = ScaleSource::Bluetooth;
    mutable std::mutex scaleMutex;
    unsigned long lastScaleMeasurement = 0;
    bool scaleMeasurementReceived = false;
    static constexpr unsigned long SCALE_GRACE_PERIOD_MS = 1500;
    std::recursive_mutex processMutex;
    Process *currentProcess = nullptr;
    VolumetricMeasurementSource currentVolumetricSource = VolumetricMeasurementSource::SCALE;
    std::vector<double> weights;
    int stops = 0, dispatched = 0;
};

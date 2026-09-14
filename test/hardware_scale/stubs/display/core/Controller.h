#pragma once
#include <cstdint>
#include <functional>
#include <vector>
#include <mutex>
#include <display/core/ScaleStatus.h>
enum class VolumetricMeasurementSource { SCALE };
struct ScaleTestClient {
    using HardwareScaleCallback = std::function<void(bool available, int32_t left, int32_t right, bool configError)>;
    HardwareScaleCallback receive;
    bool enabled = false;
    uint32_t clock = 0, left = 0, right = 0;
    void onHardwareScale(HardwareScaleCallback cb) { receive = cb; }
    void configureHardwareScale(bool value, uint32_t c, uint32_t l, uint32_t r) { enabled = value; clock = c; left = l; right = r; }
};
struct ScaleTestSettings {
    bool active = true;
    int clock = 17, left = 18, right = 39;
    bool isHardwareScaleActive() const { return active; }
    int getHardwareScaleClock() const { return clock; }
    int getHardwareScaleLeft() const { return left; }
    int getHardwareScaleRight() const { return right; }
};
class Controller {
  public:
    bool active = false, updating = false;
    int unavailable = 0;
    ScaleTestClient client;
    ScaleTestSettings settings;
    std::recursive_mutex processMutex;
    std::recursive_mutex &getProcessLock() { return processMutex; }
    ScaleTestSettings &getSettings() { return settings; }
    std::vector<double> weights;
    std::function<void()> tareSelected;
    ScaleTestClient *getClientController() { return &client; }
    bool isActive() const { return active; }
    bool isUpdating() const { return updating; }
    void onScaleMeasurement(ScaleSource source, double weight) { if (source == ScaleSource::Wired) weights.push_back(weight); }
    void onScaleDisconnected(ScaleSource) {}
    void tareScale() { tareSelected(); }
    void onScaleUnavailable() { ++unavailable; }
};

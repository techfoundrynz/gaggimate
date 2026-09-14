#include "HardwareScalePlugin.h"
#include <display/core/Controller.h>
#include <display/core/PluginManager.h>
#include <cstdio>

HardwareScalePlugin HardwareScales;

void HardwareScalePlugin::setup(Controller *c, PluginManager *manager) {
    controller = c;
    preferences.begin("hw_scale", false);
    enabled = c->getSettings().isHardwareScaleActive();
    clockPin = c->getSettings().getHardwareScaleClock();
    leftPin = c->getSettings().getHardwareScaleLeft();
    rightPin = c->getSettings().getHardwareScaleRight();
    const String saved = preferences.getString("calibration", "");
    int savedClock = -1, savedLeft = -1, savedRight = -1;
    if (sscanf(saved.c_str(), "%d,%d,%d;%lf,%lf,%lf,%lf", &savedClock, &savedLeft, &savedRight,
               &calibration.left, &calibration.right, &zeroLeft, &zeroRight) != 7 ||
        savedClock != clockPin || savedLeft != leftPin || savedRight != rightPin ||
        !calibration.valid() || !std::isfinite(zeroLeft) || !std::isfinite(zeroRight)) {
        calibration = {};
        zeroLeft = zeroRight = 0;
    }
    tareLeft = zeroLeft;
    tareRight = zeroRight;
    c->getClientController()->onHardwareScale([this](bool ok, int32_t left, int32_t right, bool error) { receive(ok, left, right, error); });
    manager->on("controller:bluetooth:connect", [this](const Event &) {
        invalidate();
        controller->getClientController()->configureHardwareScale(isEnabled(), clockPin, leftPin, rightPin);
    });
    manager->on("controller:bluetooth:disconnect", [this](const Event &) { invalidate(); });
}

bool HardwareScalePlugin::isEnabled() const {
    std::lock_guard<std::mutex> lock(mutex);
    return enabled;
}

bool HardwareScalePlugin::fresh() const { return enabled && available && millis() - lastReading < 1000; }

gm::ScaleStatus HardwareScalePlugin::getScaleStatus() const {
    std::lock_guard<std::mutex> lock(mutex);
    const bool connected = fresh();
    return {ScaleSource::Wired, enabled, connected, connected && calibration.valid() && calibrationStage == 0, -1};
}

void HardwareScalePlugin::invalidate() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        available = false;
        configError = false;
        sampleCount = sampleCursor = 0;
        calibrationStage = 0;
    }
    controller->onScaleDisconnected(ScaleSource::Wired);
}

void HardwareScalePlugin::receive(bool ok, int32_t left, int32_t right, bool error) {
    double measurement = 0;
    bool publish = false;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!enabled) return;
        configError = error;
        if (!ok || (available && millis() - lastReading >= 1000)) {
            available = false;
            sampleCount = sampleCursor = 0;
            calibrationStage = 0;
        }
        if (!ok) return;
        available = true;
        lastReading = millis();
        samples[sampleCursor][0] = left;
        samples[sampleCursor][1] = right;
        sampleCursor = (sampleCursor + 1) % 12;
        if (sampleCount < 12) ++sampleCount;
        // A short three-sample mean smooths ADC noise without a long settling lag.
        double a = 0, b = 0;
        const size_t n = sampleCount < 3 ? sampleCount : 3;
        for (size_t i = 0; i < n; ++i) {
            const size_t pos = (sampleCursor + 11 - i) % 12;
            a += samples[pos][0];
            b += samples[pos][1];
        }
        weight = calibration.weight(a / n - tareLeft, b / n - tareRight);
        publish = calibration.valid() && calibrationStage == 0 && std::isfinite(weight);
        measurement = weight;
    }
    if (publish) controller->onScaleMeasurement(ScaleSource::Wired, measurement);
}

void HardwareScalePlugin::tare() {
    std::lock_guard<std::mutex> lock(mutex);
    if (!fresh() || !calibration.valid() || calibrationStage != 0 || sampleCount == 0) return;
    const size_t latest = (sampleCursor + 11) % 12;
    tareLeft = samples[latest][0];
    tareRight = samples[latest][1];
    weight = 0;
    sampleCount = sampleCursor = 0; // Don't blend pre-tare readings into the next shot.
}

bool HardwareScalePlugin::average(double &left, double &right) const {
    if (!fresh() || sampleCount < 12) return false;
    left = right = 0;
    for (const auto &s : samples) { left += s[0]; right += s[1]; }
    left /= 12;
    right /= 12;
    // Reject moving loads (raw-count tolerance also works before first calibration).
    for (const auto &s : samples)
        if (std::abs(s[0] - left) > 2000 || std::abs(s[1] - right) > 2000) return false;
    return true;
}

String HardwareScalePlugin::command(const String &action, double mass) {
    if (controller == nullptr || !isEnabled()) return "Enable Hardware Scales in Plugins, then save and restart";
    std::unique_lock<std::recursive_mutex> processLock(controller->getProcessLock());
    if (controller->isActive() || controller->isUpdating()) return "Stop the current process before changing the scales";
    if (action == "tare") {
        if (!isReady()) return "Calibrate and connect both scales before taring";
        controller->tareScale();
        return "";
    }
    std::lock_guard<std::mutex> lock(mutex);
    if (action == "cancel") { calibrationStage = 0; return ""; }
    double a, b;
    if (!average(a, b)) return "Wait for both scales to be connected and steady for two seconds";
    if (action == "empty") {
        emptyLeft = a;
        emptyRight = b;
        calibrationStage = 1;
    } else if (action == "left" && calibrationStage == 1) {
        if (!std::isfinite(mass) || mass < 10 || mass > 500) return "Use a known mass between 10 and 500 grams";
        knownMass = mass;
        loadedLeft = a - emptyLeft;
        loadedRight = b - emptyRight;
        calibrationStage = 2;
    } else if (action == "right" && calibrationStage == 2) {
        ScaleCalibration result;
        if (!ScaleCalibration::solve(loadedLeft, loadedRight, a - emptyLeft, b - emptyRight, knownMass, result))
            return "Calibration failed: use the same mass at two distinct positions, near each support";
        char saved[128];
        snprintf(saved, sizeof(saved), "%d,%d,%d;%.17g,%.17g,%.17g,%.17g", clockPin, leftPin, rightPin,
                 result.left, result.right, emptyLeft, emptyRight);
        if (preferences.putString("calibration", saved) == 0) return "Unable to save calibration";
        calibration = result;
        zeroLeft = tareLeft = emptyLeft;
        zeroRight = tareRight = emptyRight;
        calibrationStage = 0;
    } else {
        return "Follow the calibration steps in order";
    }
    sampleCount = sampleCursor = 0;
    return "";
}

void HardwareScalePlugin::writeStatus(JsonDocument &doc) const {
    std::lock_guard<std::mutex> lock(mutex);
    doc["enabled"] = enabled;
    doc["configError"] = configError;
    doc["clockPin"] = clockPin;
    doc["leftPin"] = leftPin;
    doc["rightPin"] = rightPin;
    doc["available"] = fresh();
    doc["calibrated"] = calibration.valid();
    doc["stage"] = calibrationStage;
    doc["samplesReady"] = fresh() && sampleCount == 12;
    doc["mass"] = knownMass;
    if (fresh() && calibration.valid() && calibrationStage == 0) doc["weight"] = weight;
    else doc["weight"] = nullptr;
}

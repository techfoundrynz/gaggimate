#pragma once

#include <ArduinoJson.h>
#include <Preferences.h>
#include <display/core/Plugin.h>
#include <display/core/ScaleCalibration.h>
#include <display/core/ScaleStatus.h>
#include <mutex>

class HardwareScalePlugin : public Plugin {
  public:
    void setup(Controller *controller, PluginManager *manager) override;
    void loop() override {} // scale-loss handling is shared by the controller
    bool isEnabled() const;
    gm::ScaleStatus getScaleStatus() const;
    bool isReady() const { return getScaleStatus().ready; }
    void tare();
    void writeStatus(JsonDocument &doc) const;
    String command(const String &action, double mass = 0);

  private:
    void receive(bool available, int32_t left, int32_t right, bool configError);
    bool fresh() const;
    bool average(double &left, double &right) const;
    void invalidate();
    mutable std::mutex mutex;
    Controller *controller = nullptr;
    Preferences preferences;
    bool enabled = false, available = false;
    bool configError = false;
    int clockPin = 17, leftPin = 18, rightPin = 39;
    uint32_t lastReading = 0;
    ScaleCalibration calibration;
    double zeroLeft = 0, zeroRight = 0;
    double tareLeft = 0, tareRight = 0;
    double weight = 0;
    int32_t samples[12][2]{};
    size_t sampleCount = 0, sampleCursor = 0;
    int calibrationStage = 0;
    double emptyLeft = 0, emptyRight = 0, loadedLeft = 0, loadedRight = 0, knownMass = 0;
};

extern HardwareScalePlugin HardwareScales;

// Simulated controller board: a small thermal + hydraulic model that reacts to
// the boiler/pump/relay commands the display sends and emits sensor telemetry.
#pragma once

#include "GaggiMateComm.h"
#include <cstdint>
#include <functional>

class MockController {
  public:
    using SensorFn = std::function<void(float temp, float pressure, float puckFlow, float pumpFlow, float puckResistance,
                                        float pumpPower, float heaterPower, float waterPumped)>;
    using VolumetricFn = std::function<void(float volume)>;
    using TofFn = std::function<void(uint32_t distance)>;
    using HardwareScaleFn = std::function<void(bool available, int32_t left, int32_t right, bool configError)>;

    void begin();
    void update();

    void setBoiler(const BoilerCommand &c);
    void setPump(const PumpCommand &c);
    void setRelay(const RelayCommand &c);
    void tareScale() { weight = 0.0f; }
    void configureHardwareScale(bool enabled, uint32_t clock, uint32_t left, uint32_t right);
    void setScaleLoad(double mass, double leftShare) {
        scaleMass = mass;
        scaleLeftShare = leftShare;
        collectedMass = 0;
    }
    double getScaleMass() const { return scaleMass; }
    double getScaleLeftShare() const { return scaleLeftShare; }
    void setGrinderRunning(bool running) { grinderRunning = running; }

    SensorFn onSensor;
    VolumetricFn onVolumetric;
    TofFn onTof;
    HardwareScaleFn onHardwareScale;

  private:
    bool active = false;
    uint32_t lastUpdateMs = 0;
    uint32_t lastSensorMs = 0;
    uint32_t lastTofMs = 0;
    uint32_t lastScaleMs = 0;
    bool scaleEnabled = false;
    bool scaleConfigError = false;
    bool grinderRunning = false;
    double scaleMass = 0;
    double scaleLeftShare = 0.5;
    double collectedMass = 0;

    float ambient = 21.0f;
    float temperature = 21.0f;
    float targetTemp = 0.0f; // boiler setpoint (0 = off)

    PumpControlMode pumpMode = PumpControlMode::Power;
    float pumpPower = 0.0f;      // 0..100
    float targetPressure = 0.0f; // bar
    float targetFlow = 0.0f;     // ml/s
    bool brewValveOpen = false;

    float pressure = 0.0f; // bar
    float flow = 0.0f;     // ml/s
    float weight = 0.0f;   // g accumulated on the scale
};

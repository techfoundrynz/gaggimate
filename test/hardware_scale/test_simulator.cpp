#include <cassert>
#include <cmath>
#include <cstdio>

#define constrain(value, low, high) ((value) < (low) ? (low) : ((value) > (high) ? (high) : (value)))
#include "../../sim/comms/MockController.cpp"
#include "../../src/display/core/ScaleCalibration.h"

static unsigned long now = 100;
unsigned long millis() { return now; }

int main() {
    MockController mock;
    int reports = 0;
    bool available = false, error = false;
    int32_t left = 0, right = 0;
    mock.onHardwareScale = [&](bool ok, int32_t a, int32_t b, bool badPins) {
        ++reports;
        available = ok;
        error = badPins;
        left = a;
        right = b;
    };
    auto tick = [&] {
        now += 50;
        mock.update();
    };
    mock.begin();
    tick();
    assert(reports == 0);
    mock.configureHardwareScale(true, 17, 18, 39);
    tick();
    assert(available && !error);
    const auto emptyLeft = left, emptyRight = right;
    mock.setScaleLoad(100, 0.8);
    tick();
    const auto loadedLeft = left - emptyLeft, loadedRight = right - emptyRight;
    mock.setScaleLoad(100, 0.2);
    tick();
    ScaleCalibration calibration;
    assert(ScaleCalibration::solve(loadedLeft, loadedRight, left - emptyLeft, right - emptyRight, 100, calibration));
    auto weight = [&] { return calibration.weight(left - emptyLeft, right - emptyRight); };
    for (double position : {0.2, 0.5, 0.8}) {
        mock.setScaleLoad(125, position);
        tick();
        assert(std::abs(weight() - 125) < 0.02);
    }
    // The flow-estimation tare command must not remove a physical load.
    mock.tareScale();
    tick();
    assert(std::abs(weight() - 125) < 0.02);
    mock.setScaleLoad(0, 0.5);
    mock.setGrinderRunning(true);
    for (int i = 0; i < 20; ++i)
        tick();
    assert(std::abs(weight() - 3) < 0.02);
    mock.setGrinderRunning(false);
    mock.setPump({0, PumpControlMode::Power, 80, 0, 0});
    mock.setRelay({0, true});
    for (int i = 0; i < 40; ++i)
        tick();
    assert(weight() > 5);
    mock.configureHardwareScale(true, 17, 17, 39);
    tick();
    assert(!available && error);
    mock.configureHardwareScale(false, 0, 0, 0);
    const int beforeDisable = reports;
    tick();
    assert(reports == beforeDisable);
    puts("Simulated HX711 calibration, tare, delivery and enable/disable checks passed");
}

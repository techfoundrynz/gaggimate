#include <cassert>
#include <cmath>
#include <cstdio>
#include <atomic>
#include <functional>
#include <utility>
#define private public
#include "../../lib/GaggiMateController/src/peripherals/DualHx711.h"
#undef private
#include "../../lib/GaggiMateController/src/peripherals/DualHx711.cpp"
#include "../../src/display/core/ScaleCalibration.h"
#include "../../lib/GaggiMateController/src/peripherals/HardwareScalePins.h"

bool critical = false;
static bool ready[2] = {true, true};
static bool stuckLow = false;
static uint32_t values[2] = {};
static int pulses = 0, modes = 0, clockLevel = 0;
static unsigned clockHighUs = 0;
void pinMode(int, int) { ++modes; }
void digitalWrite(int pin, int level) {
    assert(pin == 17);
    clockLevel = level;
    if (level) { ++pulses; clockHighUs = 0; }
}
int digitalRead(int pin) {
    const int channel = pin == 18 ? 0 : 1;
    if (!clockLevel) return pulses >= 25 ? !stuckLow : (ready[channel] ? 0 : 1);
    assert(critical && pulses >= 1 && pulses <= 24);
    return (values[channel] >> (24 - pulses)) & 1;
}
void delayMicroseconds(unsigned us) {
    if (clockLevel) {
        clockHighUs += us;
        if (critical) assert(clockHighUs <= 50);
    }
}
unsigned long millis() { return 0; }
void vTaskDelay(unsigned) {}
int xTaskCreate(void (*)(void *), const char *, unsigned, void *, unsigned, void *) { return pdPASS; }

int main() {
    DualHx711 adc(17, 18, 39, [](bool, int32_t, int32_t) {});
    assert(adc.setup());
    assert(modes == 0 && pulses == 0); // Disabled by default: no GPIO ownership.
    DualHx711 invalid(17, 17, 39, [](bool, int32_t, int32_t) {});
    assert(!invalid.setup());
    int32_t left = 0, right = 0;
    ready[1] = false;
    assert(!adc.read(left, right) && pulses == 0);
    ready[0] = false; ready[1] = true;
    assert(!adc.read(left, right) && pulses == 0);
    ready[0] = true;
    values[0] = 123456; values[1] = (-234567) & 0xffffff;
    assert(adc.read(left, right));
    assert(left == 123456 && right == -234567);
    assert(pulses == 25 && clockLevel == 0 && !critical);
    pulses = 0;
    values[0] = 0x800000; values[1] = 0x7fffff;
    assert(adc.read(left, right));
    assert(left == -8388608 && right == 8388607);
    pulses = 0;
    stuckLow = true;
    assert(!adc.read(left, right));

    ScaleCalibration calibration;
    // Left cell 100 counts/g; right cell 200 counts/g. The tray shares the mass.
    assert(ScaleCalibration::solve(8000, 4000, 2000, 16000, 100, calibration));
    assert(std::abs(calibration.left - .01) < 1e-10);
    assert(std::abs(calibration.right - .005) < 1e-10);
    assert(std::abs(calibration.weight(5000, 10000) - 100) < 1e-8);
    assert(std::abs(calibration.weight(-1000, -2000) + 20) < 1e-8);
    // One cell wired with inverted polarity still calibrates correctly.
    assert(ScaleCalibration::solve(-8000, 4000, -2000, 16000, 100, calibration));
    assert(std::abs(calibration.weight(-5000, 10000) - 100) < 1e-8);
    auto previous = calibration;
    assert(!ScaleCalibration::solve(5000, 10000, 5000, 10000, 100, calibration));
    assert(!ScaleCalibration::solve(5000, 10000, 5001, 9999, 100, calibration));
    assert(!ScaleCalibration::solve(8000, 4000, 2000, 16000, 0, calibration));
    assert(!ScaleCalibration::solve(8000, 4000, 2000, 16000, NAN, calibration));
    assert(!ScaleCalibration::solve(INFINITY, 4000, 2000, 16000, 100, calibration));
    assert(calibration.left == previous.left && calibration.right == previous.right);
    assert(!ScaleCalibration{}.valid());
    for (const ControllerConfig &board : {GM_STANDARD_REV_1X, GM_STANDARD_REV_2X, GM_STANDARD_REV_3X,
                                          GM_PRO_REV_1x, GM_PRO_LEGO, GM_PRO_REV_11}) {
        assert(hardwareScalePinsAvailable(board, 17, 18, 39, false));
        assert(!hardwareScalePinsAvailable(board, board.heaterPin, 18, 39, false));
        assert(!hardwareScalePinsAvailable(board, 17, board.maxMisoPin, 39, false));
        assert(!hardwareScalePinsAvailable(board, 17, 18, board.ext2Pin, false));
        assert(!hardwareScalePinsAvailable(board, 17, 17, 39, false));
        assert(!hardwareScalePinsAvailable(board, 26, 18, 39, false));
        assert(!hardwareScalePinsAvailable(board, 19, 18, 39, false));
        assert(!hardwareScalePinsAvailable(board, 256, 18, 39, false));
    }
    assert(hardwareScalePinsAvailable(GM_PRO_LEGO, 12, 13, 39, false));
    puts("Dual HX711 readiness, pulse timing, signed reads and tray calibration checks passed");
}

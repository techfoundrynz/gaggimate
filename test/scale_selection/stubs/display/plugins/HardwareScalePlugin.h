#pragma once
#include <display/core/ScaleStatus.h>
struct TestHardwareScale {
    gm::ScaleStatus status{ScaleSource::Wired, true, true, true, -1};
    int tares = 0;
    gm::ScaleStatus getScaleStatus() const { return status; }
    void tare() { ++tares; }
};
inline TestHardwareScale HardwareScales;

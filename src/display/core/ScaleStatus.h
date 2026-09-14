#pragma once

enum class ScaleSource { Bluetooth, Wired };

namespace gm {
struct ScaleStatus {
    ScaleSource source = ScaleSource::Bluetooth;
    bool configured = false;
    bool connected = false;
    bool ready = false;
    int batteryPercent = -1; // unavailable (wired scales have no battery)
};
} // namespace gm

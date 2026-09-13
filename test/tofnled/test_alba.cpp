#include <cassert>
#include <cstdio>
#include "peripherals/LedController.h"

int main() {
    std::recursive_mutex mutex;
    SoftWire wire; // ToFnLED also present: Alba must take priority.
    PCA9634::present = true;
    LedController alba(&wire, mutex);
    alba.setChannel(0, 255);
    alba.disable();
    assert(PCA9634::writes.empty()); // No hardware writes before initialization.

    assert(alba.isAvailable());
    assert(PCA9634::beginCalls == 1);
    assert(wire.frames.empty());
    assert(PCA9634::mode1 == PCA963X_MODE1_NONE);
    assert(PCA9634::mode2 == PCA963X_MODE2_TOTEMPOLE);
    assert(PCA9634::driverMode == PCA963X_LEDPWM);
    const std::vector<std::pair<int, int>> off = {{-1, 0}, {4, 255}, {5, 255}};
    assert(PCA9634::writes == off); // Preserve the external outputs' off polarity.

    PCA9634::writes.clear();
    assert(alba.isAvailable());
    assert(PCA9634::beginCalls == 1);
    assert(PCA9634::writes.empty()); // Availability must not reset lighting.
    for (int channel = 0; channel < 8; ++channel) alba.setChannel(channel, channel * 30);
    assert(PCA9634::writes.size() == 8);
    for (int channel = 0; channel < 8; ++channel)
        assert(PCA9634::writes[channel] == std::make_pair(channel, channel * 30));
    alba.setChannel(8, 255);
    alba.setChannel(255, 255);
    assert(PCA9634::writes.size() == 8);

    PCA9634::writes.clear();
    alba.disable();
    assert(PCA9634::writes == off);
    PCA9634::writes.clear();
    alba.setChannel(3, 255); // White is a separate channel on Alba.
    assert((PCA9634::writes == std::vector<std::pair<int, int>>{{3, 255}}));
    PCA9634::writes.clear();
    alba.setup();
    assert(PCA9634::beginCalls == 1);
    assert(PCA9634::writes == off);
    assert(wire.frames.empty()); // No NCP5623 probes or commands on any Alba path.
    puts("Alba initialization, priority, channels, disable and resume checks passed");
}

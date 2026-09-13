#include <cassert>
#include <cstdio>
#include "peripherals/LedController.h"

int main() {
    std::recursive_mutex mutex;
    SoftWire wire;
    LedController led(&wire, mutex);
    assert(led.isAvailable());
    assert((wire.frames == std::vector<std::vector<uint8_t>>{
        {0x38}, {0x38, 0x00}, {0x38, 0x40}, {0x38, 0x60}, {0x38, 0x80}, {0x38, 0x3D}}));
    wire.frames.clear();
    led.setChannel(0, 255);
    assert((wire.frames == std::vector<std::vector<uint8_t>>{{0x38, 0x5F}, {0x38, 0x60}, {0x38, 0x80}}));
    wire.frames.clear();
    led.setChannel(3, 128); // White mixes into every channel; red saturates.
    assert((wire.frames == std::vector<std::vector<uint8_t>>{{0x38, 0x5F}, {0x38, 0x70}, {0x38, 0x90}}));
    wire.frames.clear();
    for (int channel = 4; channel < 9; ++channel) led.setChannel(channel, 255);
    assert(wire.frames.empty());
    led.disable();
    assert((wire.frames == std::vector<std::vector<uint8_t>>{{0x38, 0x40}, {0x38, 0x60}, {0x38, 0x80}}));
    wire.frames.clear();
    led.setChannel(2, 255); // Disable cleared the cached white and red too.
    assert((wire.frames == std::vector<std::vector<uint8_t>>{{0x38, 0x40}, {0x38, 0x60}, {0x38, 0x9F}}));

    SoftWire absent;
    absent.present = false;
    LedController missing(&absent, mutex);
    assert(!missing.isAvailable());
    absent.frames.clear();
    missing.disable();
    missing.setChannel(0, 255);
    assert(absent.frames.empty());
    for (int failure = 2; failure <= 6; ++failure) {
        SoftWire faulty;
        faulty.failAt = failure;
        LedController failed(&faulty, mutex);
        assert(!failed.isAvailable());
        assert((faulty.frames.back() == std::vector<uint8_t>{0x38, 0x00}));
    }
    puts("ToFnLED protocol, RGBW mapping, detection and error checks passed");
}

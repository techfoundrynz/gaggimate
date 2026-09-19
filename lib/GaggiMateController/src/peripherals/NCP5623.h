#ifndef NCP5623_H
#define NCP5623_H

#include <SoftWire.h>
#include <stdint.h>

// NCP5623/D: each transaction contains one command byte, not register + data.
class NCP5623 {
  public:
    explicit NCP5623(SoftWire *wire) : wire(wire) {}

    bool begin() {
        wire->beginTransmission(0x38);
        if (wire->endTransmission() != 0) {
            return false;
        }
        // Clear retained PWM before enabling current (also handles MCU-only resets).
        if (!command(0x00) || !command(0x40) || !command(0x60) || !command(0x80) || !command(0x3D)) {
            command(0x00);
            return false;
        }
        // Current step 29: approximately 10.6 mA/channel with ToFnLED's 68k IREF.
        return true;
    }

    bool setRgbw(uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
        bool ok = command(0x40 | pwm(r, w));
        ok = command(0x60 | pwm(g, w)) && ok;
        ok = command(0x80 | pwm(b, w)) && ok;
        return ok;
    }

  private:
    SoftWire *wire;

    static uint8_t pwm(uint8_t color, uint8_t white) {
        unsigned sum = static_cast<unsigned>(color) + white;
        if (sum > 255) {
            sum = 255;
        }
        return static_cast<uint8_t>((sum * 31 + 127) / 255);
    }

    bool command(uint8_t value) {
        wire->beginTransmission(0x38);
        wire->write(value);
        return wire->endTransmission() == 0;
    }
};

#endif

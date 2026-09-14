#pragma once

#include <Arduino.h>
#include <atomic>
#include <functional>

// Two HX711s on a shared PD_SCK, each with its own DOUT. No I2C/SPI peripheral is used.
class DualHx711 {
  public:
    using Callback = std::function<void(bool, int32_t, int32_t)>;
    DualHx711(uint8_t clock, uint8_t left, uint8_t right, Callback callback);
    bool setup();
    void configure(bool enabled, uint8_t clock, uint8_t left, uint8_t right) {
        requested.store(enabled ? 0x1000000u | (uint32_t(clock) << 16) | (uint32_t(left) << 8) | right : 0);
    }

  private:
    uint8_t clockPin, leftPin, rightPin;
    Callback callback;
    std::atomic<uint32_t> requested{0};
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
    static void task(void *arg);
    void run();
    void reset();
    bool read(int32_t &left, int32_t &right);
};

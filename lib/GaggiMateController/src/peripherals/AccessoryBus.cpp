#include "AccessoryBus.h"
#include <Arduino.h>
#include <driver/gpio.h>

bool prepareAccessoryBus(uint8_t sda, uint8_t scl) {
    if (sda == scl) return false; // Older boards do not have an accessory port.
    gpio_reset_pin(static_cast<gpio_num_t>(sda));
    gpio_reset_pin(static_cast<gpio_num_t>(scl));
    pinMode(sda, INPUT_PULLUP);
    pinMode(scl, INPUT_PULLUP);
    for (int i = 0; i < 9; ++i) {
        digitalWrite(scl, LOW);
        pinMode(scl, OUTPUT);
        delayMicroseconds(100);
        pinMode(scl, INPUT_PULLUP);
        delayMicroseconds(100);
    }
    // STOP condition after releasing a slave left mid-transaction.
    digitalWrite(sda, LOW);
    pinMode(sda, OUTPUT);
    delayMicroseconds(100);
    pinMode(sda, INPUT_PULLUP);
    delayMicroseconds(100);
    return digitalRead(sda) && digitalRead(scl);
}

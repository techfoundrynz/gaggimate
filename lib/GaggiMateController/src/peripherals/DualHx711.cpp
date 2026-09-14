#include "DualHx711.h"
#include <utility>

DualHx711::DualHx711(uint8_t clock, uint8_t left, uint8_t right, Callback cb)
    : clockPin(clock), leftPin(left), rightPin(right), callback(std::move(cb)) {}

bool DualHx711::setup() {
    if (clockPin == leftPin || clockPin == rightPin || leftPin == rightPin) return false;
    return xTaskCreate(task, "HX711", 3072, this, 1, nullptr) == pdPASS;
}

void DualHx711::task(void *arg) { static_cast<DualHx711 *>(arg)->run(); }

void DualHx711::reset() {
    digitalWrite(clockPin, HIGH);
    delayMicroseconds(80); // Deliberate power-down; the falling edge resets both converters to A/128.
    digitalWrite(clockPin, LOW);
}

bool DualHx711::read(int32_t &left, int32_t &right) {
    // Never clock one converter while the other is still converting.
    if (digitalRead(leftPin) || digitalRead(rightPin)) return false;
    uint32_t a = 0, b = 0;
    // Preemption while SCK is high for >60 us would power down the ADC. Keep the
    // short 25-pulse transaction together; there is no conversion wait here.
    portENTER_CRITICAL(&mux);
    for (int bit = 0; bit < 25; ++bit) {
        digitalWrite(clockPin, HIGH);
        delayMicroseconds(1);
        if (bit < 24) {
            a = (a << 1) | digitalRead(leftPin);
            b = (b << 1) | digitalRead(rightPin);
        }
        digitalWrite(clockPin, LOW);
        delayMicroseconds(1);
    }
    // The 25th pulse starts the next conversion and DOUT must return high.
    // Otherwise a line shorted to ground would look like a valid zero forever.
    const bool converted = digitalRead(leftPin) && digitalRead(rightPin);
    portEXIT_CRITICAL(&mux);
    if (!converted) return false;
    left = static_cast<int32_t>(a & 0x800000 ? a | 0xff000000 : a);
    right = static_cast<int32_t>(b & 0x800000 ? b | 0xff000000 : b);
    return true;
}

void DualHx711::run() {
    bool enabled = false;
    uint32_t applied = 0;
    uint32_t lastReady = 0, lastReport = 0;
    for (;;) {
        const uint32_t config = requested.load();
        if (config != applied) {
            if (enabled) {
                digitalWrite(clockPin, LOW);
                pinMode(clockPin, INPUT);
                pinMode(leftPin, INPUT);
                pinMode(rightPin, INPUT);
            }
            applied = config;
            enabled = config != 0;
            if (enabled) {
                clockPin = (config >> 16) & 0xff;
                leftPin = (config >> 8) & 0xff;
                rightPin = config & 0xff;
                pinMode(leftPin, INPUT_PULLUP);
                pinMode(rightPin, INPUT_PULLUP);
                digitalWrite(clockPin, LOW);
                pinMode(clockPin, OUTPUT);
                reset();
                lastReady = millis();
            }
            callback(false, 0, 0);
        }
        if (enabled) {
            int32_t left, right;
            const uint32_t now = millis();
            if (read(left, right)) {
                lastReady = now;
                if (now - lastReport >= 50) {
                    // ADC saturation is not a usable weight.
                    const bool valid = left != -8388608 && left != 8388607 && right != -8388608 && right != 8388607;
                    callback(valid, left, right);
                    lastReport = now;
                }
            } else if (now - lastReady >= 1000) {
                callback(false, 0, 0);
                reset(); // Re-synchronise both chips after a disconnected/stuck DOUT.
                lastReady = now;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(enabled ? 5 : 100));
    }
}

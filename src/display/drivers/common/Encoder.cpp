#ifndef GAGGIMATE_SIM
#include "Encoder.h"
#include "EncoderInput.h"
#include <Arduino.h>

static EncoderConfig config;
static EncoderRotation rotation;
static EncoderButtonQueue button;
static portMUX_TYPE encoderMux = portMUX_INITIALIZER_UNLOCKED;
static volatile int16_t pendingSteps = 0;
static bool encoderReady = false;

static void sampleButton(void *) {
    TickType_t lastWake = xTaskGetTickCount();
    for (;;) {
        const bool rawPressed = digitalRead(config.buttonPin) == LOW;
        const uint32_t now = millis();
        portENTER_CRITICAL(&encoderMux);
        button.sample(rawPressed, now);
        portEXIT_CRITICAL(&encoderMux);
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(5));
    }
}

// Capture every quadrature edge; polling only at the LVGL refresh rate misses turns.
static void ARDUINO_ISR_ATTR encoderEdge() {
    portENTER_CRITICAL_ISR(&encoderMux);
    const uint8_t state = (digitalRead(config.pinA) << 1) | digitalRead(config.pinB);
    const int16_t next = pendingSteps + rotation.update(state) * config.direction;
    if (next >= -127 && next <= 127) {
        pendingSteps = next;
    }
    portEXIT_CRITICAL_ISR(&encoderMux);
}

EncoderSample readEncoder() {
    if (!encoderReady) {
        return {0, false, millis(), true};
    }
    static bool wasPressed = false;
    const bool rawPressed = digitalRead(config.buttonPin) == LOW;
    portENTER_CRITICAL(&encoderMux);
    const auto sample = button.read(millis());
    const int16_t steps = pendingSteps;
    pendingSteps = 0;
    portEXIT_CRITICAL(&encoderMux);

    const int16_t movement = (rawPressed || sample.pressed || wasPressed || sample.cancelled) ? 0 : steps;
    wasPressed = sample.pressed;
    return {movement, sample.pressed, sample.at, sample.cancelled};
}

void beginEncoder(const EncoderConfig &pins) {
    if (encoderReady || pins.pinA < 0 || pins.pinB < 0 || pins.buttonPin < 0) {
        return;
    }
    if ((pins.stepsPerDetent != 2 && pins.stepsPerDetent != 4) ||
        (pins.direction != 1 && pins.direction != -1) || pins.pinA == pins.pinB ||
        pins.pinA == pins.buttonPin || pins.pinB == pins.buttonPin) {
        ESP_LOGE("Encoder", "Invalid encoder configuration");
        return;
    }
    config = pins;
    rotation = EncoderRotation(config.stepsPerDetent);
    pinMode(config.pinA, INPUT_PULLUP);
    pinMode(config.pinB, INPUT_PULLUP);
    pinMode(config.buttonPin, INPUT_PULLUP);
    rotation.reset((digitalRead(config.pinA) << 1) | digitalRead(config.pinB));
    attachInterrupt(digitalPinToInterrupt(config.pinA), encoderEdge, CHANGE);
    attachInterrupt(digitalPinToInterrupt(config.pinB), encoderEdge, CHANGE);

    encoderReady = xTaskCreatePinnedToCore(sampleButton, "EncoderButton", 2048, nullptr, 2, nullptr, 0) == pdPASS;
    if (!encoderReady) {
        detachInterrupt(digitalPinToInterrupt(config.pinA));
        detachInterrupt(digitalPinToInterrupt(config.pinB));
        ESP_LOGE("Encoder", "Could not start button sampling task");
    }
}
#endif

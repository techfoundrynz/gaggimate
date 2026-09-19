#pragma once
#include <stdint.h>

struct EncoderConfig {
    int8_t pinA = -1;
    int8_t pinB = -1;
    int8_t buttonPin = -1;
    uint8_t stepsPerDetent = 4;
    int8_t direction = 1;
};

struct EncoderSample { int16_t steps; bool pressed; uint32_t at; bool cancelled; };
void beginEncoder(const EncoderConfig &config);
EncoderSample readEncoder();

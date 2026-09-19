#pragma once

#include <stdint.h>

// Hardware-independent decoding, shared with the native tests.
class EncoderRotation {
  public:
    explicit EncoderRotation(uint8_t stepsPerDetent = 4) : stepsPerDetent(stepsPerDetent) {}

    void reset(uint8_t state) {
        previous = state & 3;
        partial = 0;
    }

    int8_t update(uint8_t state) {
        state &= 3;
        const uint8_t changed = previous ^ state;
        if (changed == 3) {
            partial = 0; // Both pins changed: a missed edge, not a valid step.
        } else if (changed != 0) {
            // Positive sequence: 00 -> 01 -> 11 -> 10 -> 00.
            partial += ((previous & 1) ^ ((state >> 1) & 1)) ? -1 : 1;
        }
        previous = state;
        if (partial >= stepsPerDetent) {
            partial = 0;
            return 1;
        }
        if (partial <= -stepsPerDetent) {
            partial = 0;
            return -1;
        }
        return 0;
    }

  private:
    uint8_t previous = 0;
    int8_t partial = 0;
    uint8_t stepsPerDetent;
};

class EncoderButton {
  public:
    bool update(bool pressed, uint32_t now) {
        if (pressed != raw) {
            raw = pressed;
            changedAt = now;
        }
        if (uint32_t(now - changedAt) >= 30) {
            stable = raw;
            if (!stable) {
                armed = true; // A button held during boot must first be released.
            }
        }
        return armed && stable;
    }

  private:
    bool raw = true;
    bool stable = true;
    bool armed = false;
    uint32_t changedAt = 0;
};

// Sample independently of drawing. Preserve both edges of a short tap until the
// UI consumes them, with their original times so drawing delays cannot turn a
// short tap into a long hold. Caller serializes sample/read.
class EncoderButtonQueue {
  public:
    struct Sample { bool pressed; uint32_t at; bool cancelled; };
    void sample(bool rawPressed, uint32_t now) {
        const bool pressed = debounce.update(rawPressed, now);
        if (pressed == state) {
            return;
        }
        state = pressed;
        if (count == Capacity) {
            head = count = 0;
            overflow = true;
        }
        if (!overflow) {
            events[(head + count) % Capacity] = {pressed, now, false};
            ++count;
        }
    }
    Sample read(uint32_t now) {
        if (overflow) {
            if (!state) {
                overflow = false;
            }
            return {false, now, true}; // Cancel rather than replay an incomplete gesture.
        }
        if (!count) {
            return {state, now, false};
        }
        const auto result = events[head];
        head = (head + 1) % Capacity;
        --count;
        return result;
    }
  private:
    static constexpr unsigned Capacity = 16;
    EncoderButton debounce;
    Sample events[Capacity]{};
    unsigned head = 0, count = 0;
    bool state = false, overflow = false;
};

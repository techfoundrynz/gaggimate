#include "DigitalInput.h"

DigitalInput::DigitalInput(uint8_t pin, const input_callback_t &callback, const int debounce_count,
                           const int release_debounce_count)
    : _pin(pin), _debounce_count(debounce_count),
      _release_debounce_count(release_debounce_count > 0 ? release_debounce_count : debounce_count), _callback(callback) {}

DigitalInput::DigitalInput(uint8_t pin, const input_callback_t &callback, Timing timing)
    : DigitalInput(pin, callback) {
    _poll_ms = timing.pollMs;
    _debounce_ms = timing.debounceMs;
}

void DigitalInput::setup() {
    pinMode(_pin, INPUT_PULLUP);
    xTaskCreate(loopTask, "DigitalInput::loop", configMINIMAL_STACK_SIZE * 4, this, 1, &taskHandle);
}

void DigitalInput::loop() {
    const uint32_t now = millis();
    if (const int state = digitalRead(_pin); state != _last_state) {
        _stable_counter = 1;
        _last_state = state;
        _changed_at = now;
        ESP_LOGD("DigitalInput", "Pin %u raw state changed: %d", _pin, !_last_state);
    } else {
        _stable_counter++;
    }
    const auto threshold = _last_state == LOW ? _debounce_count : _release_debounce_count;
    const bool stable = _debounce_ms > 0 ? uint32_t(now - _changed_at) >= _debounce_ms : _stable_counter >= threshold;
    if (stable && _current_state != _last_state) {
        _current_state = _last_state;
        _callback(!_current_state);
        ESP_LOGI("DigitalInput", "Pin %u stable state changed: %d", _pin, !_current_state);
    }
    _stable_counter = min(_stable_counter, threshold);
}

void DigitalInput::loopTask(void *arg) {
    auto *input = static_cast<DigitalInput *>(arg);
    while (true) {
        input->loop();
        const TickType_t interval = pdMS_TO_TICKS(input->_poll_ms);
        vTaskDelay(interval > 0 ? interval : 1);
    }
}

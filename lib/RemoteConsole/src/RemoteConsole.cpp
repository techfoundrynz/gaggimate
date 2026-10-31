#include "RemoteConsole.h"
#include <Arduino.h>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#ifndef GAGGIMATE_SIM
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#endif

namespace {
std::atomic<bool> active{false};
std::atomic<uint32_t> sequence{0};
std::atomic<uint32_t> lost{0};
std::mutex bufferMutex;
remote_console::Buffer<24> buffer;

void push(const remote_console::Record &record) {
    if (!active.load())
        return;
    // Logging must never wait for a consumer (which may itself hold transport locks).
    std::unique_lock<std::mutex> lock(bufferMutex, std::try_to_lock);
    if (!lock.owns_lock()) {
        ++lost;
        return;
    }
    if (active.load())
        buffer.push(record);
}

void capture(const char *format, va_list args) {
    if (!active.load())
        return;
#ifndef GAGGIMATE_SIM
    if (xPortInIsrContext())
        return;
#endif
    remote_console::Record record;
    record.timestamp = millis();
    record.sequence = ++sequence;
    const int length = vsnprintf(record.text, sizeof(record.text), format, args);
    if (length <= 0)
        return;
    if (size_t(length) >= sizeof(record.text))
        memcpy(record.text + sizeof(record.text) - 4, "...", 4);
    push(record);
}

#ifndef GAGGIMATE_SIM
vprintf_like_t previousSink = nullptr;

int idfSink(const char *format, va_list args) {
    va_list copy;
    va_copy(copy, args);
    capture(format, copy);
    va_end(copy);
    return previousSink ? previousSink(format, args) : vprintf(format, args);
}
#endif
} // namespace

namespace remote_console {
void begin() {
#ifndef GAGGIMATE_SIM
    previousSink = esp_log_set_vprintf(idfSink);
#endif
}
void setEnabled(bool value) {
    std::lock_guard<std::mutex> lock(bufferMutex);
    if (active.exchange(value) != value) {
        buffer.clear();
        lost = 0;
    }
}
bool pop(Record &record) {
    std::lock_guard<std::mutex> lock(bufferMutex);
    if (!buffer.pop(record))
        return false;
    record.dropped += lost.exchange(0);
    return true;
}
void pushRemote(const Record &record) { push(record); }
void noteDropped(uint32_t count) { lost.fetch_add(count); }
} // namespace remote_console

#ifdef GAGGIMATE_SIM
extern "C" void gm_sim_log(const char *format, ...) {
    va_list args;
    va_start(args, format);
    va_list copy;
    va_copy(copy, args);
    capture(format, copy);
    va_end(copy);
    vfprintf(stderr, format, args);
    va_end(args);
}
#else
// The ESP32-S3 framework already wraps log_printf for diagnostics. Hook its
// downstream va_list function so we coexist with that wrapper rather than replace it.
extern "C" int __real_log_printfv(const char *format, va_list args);
extern "C" int __wrap_log_printfv(const char *format, va_list args) {
    va_list copy;
    va_copy(copy, args);
    capture(format, copy);
    va_end(copy);
    return __real_log_printfv(format, args);
}
#endif

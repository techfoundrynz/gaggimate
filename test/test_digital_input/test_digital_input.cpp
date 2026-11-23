// Exercise the actual DigitalInput implementation with fake GPIO and time.
// Arduino.h resolves to the native stub configured in platformio.ini.
#include <algorithm>
#include <cstdint>
#include <functional>
#include <vector>
#include <unity.h>

using std::min;
using TickType_t = uint32_t;
using xTaskHandle = void *;
constexpr int LOW = 0;
constexpr int INPUT_PULLUP = 2;
constexpr int configMINIMAL_STACK_SIZE = 1024;
static uint32_t clockMs;
static int gpio;
uint32_t millis() { return clockMs; }
int digitalRead(uint8_t) { return gpio; }
void pinMode(uint8_t, int) {}
void xTaskCreate(void (*)(void *), const char *, int, void *, int, xTaskHandle *) {}
void vTaskDelay(TickType_t) {}
#define pdMS_TO_TICKS(ms) (ms)
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGD(...) ((void)0)

#include "peripherals/DigitalInput.cpp"

void setUp() { clockMs = 0; gpio = 1; }
void tearDown() {}

static void sample(DigitalInput &input, uint32_t at, int level) {
    clockMs = at;
    gpio = level;
    input.loop();
}

void test_noise_and_bounce_on_both_edges() {
    std::vector<bool> events;
    DigitalInput input(38, [&](bool state) { events.push_back(state); }, DigitalInput::Timing{10, 30});
    sample(input, 0, 1);
    sample(input, 29, 1);
    TEST_ASSERT_TRUE(events.empty());
    sample(input, 30, 1); // Initial state is also qualified.
    TEST_ASSERT_EQUAL(1, events.size());
    TEST_ASSERT_FALSE(events.back());
    events.clear();

    sample(input, 40, 0);
    sample(input, 60, 1); // A 20 ms glitch must not become a press.
    sample(input, 100, 1);
    TEST_ASSERT_TRUE(events.empty());
    sample(input, 110, 0);
    sample(input, 120, 1);
    sample(input, 130, 0); // Bounce restarts the stability timer.
    sample(input, 159, 0);
    TEST_ASSERT_TRUE(events.empty());
    sample(input, 160, 0);
    sample(input, 200, 0);
    TEST_ASSERT_EQUAL(1, events.size());
    TEST_ASSERT_TRUE(events.back());

    sample(input, 210, 1);
    sample(input, 230, 0); // Release glitch must not end a hold.
    sample(input, 240, 1);
    sample(input, 269, 1);
    TEST_ASSERT_EQUAL(1, events.size());
    sample(input, 270, 1);
    TEST_ASSERT_EQUAL(2, events.size());
    TEST_ASSERT_FALSE(events.back());
}

void test_time_wrap_and_initially_held_button() {
    std::vector<bool> events;
    DigitalInput input(48, [&](bool state) { events.push_back(state); }, DigitalInput::Timing{10, 30});
    sample(input, UINT32_MAX - 10, 0);
    sample(input, 18, 0); // 29 ms, spanning millis() rollover.
    TEST_ASSERT_TRUE(events.empty());
    sample(input, 19, 0);
    TEST_ASSERT_EQUAL(1, events.size());
    TEST_ASSERT_TRUE(events.back());
}

void test_legacy_sensor_sample_counts() {
    std::vector<bool> events;
    DigitalInput sensor(1, [&](bool state) { events.push_back(state); }, 3, 1);
    sample(sensor, 0, 1);
    TEST_ASSERT_EQUAL(1, events.size());
    sample(sensor, 100, 0);
    sample(sensor, 200, 0);
    TEST_ASSERT_EQUAL(1, events.size());
    sample(sensor, 300, 0);
    TEST_ASSERT_EQUAL(2, events.size());
    TEST_ASSERT_TRUE(events.back());
    sample(sensor, 400, 1);
    TEST_ASSERT_EQUAL(3, events.size());
    TEST_ASSERT_FALSE(events.back());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_noise_and_bounce_on_both_edges);
    RUN_TEST(test_time_wrap_and_initially_held_button);
    RUN_TEST(test_legacy_sensor_sample_counts);
    return UNITY_END();
}

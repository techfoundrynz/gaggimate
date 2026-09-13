#include <unity.h>
#include "display/core/ButtonHandler.cpp"
#include "display/ui/default/EncoderControls.h"
#include "display/drivers/common/EncoderInput.h"
void setUp() {}
void tearDown() {}
using Event = EncoderPress::Event;
static void sampleFor(EncoderButtonQueue &queue, bool pressed, uint32_t start, uint32_t end) {
    for (uint32_t now = start; now <= end; now += 5) queue.sample(pressed, now);
}
void test_tap_between_ui_frames_is_preserved() {
    EncoderButtonQueue queue;
    EncoderPress press;
    sampleFor(queue, false, 0, 40);
    sampleFor(queue, true, 50, 110);
    sampleFor(queue, false, 115, 160);
    // The UI never polled during this complete physical tap.
    auto down = queue.read(4000);
    auto up = queue.read(4025);
    TEST_ASSERT_TRUE(down.pressed);
    TEST_ASSERT_FALSE(up.pressed);
    TEST_ASSERT_TRUE(press.update(down.pressed, down.at, 1) == Event::None);
    TEST_ASSERT_TRUE(press.update(up.pressed, up.at, 1) == Event::Click);
    auto idle = queue.read(4050);
    TEST_ASSERT_TRUE(press.update(idle.pressed, idle.at, 1) == Event::None);
}
void test_queue_overflow_cancels_incomplete_gesture() {
    EncoderButtonQueue queue;
    sampleFor(queue, false, 0, 40);
    for (uint32_t i = 0; i < 10; ++i) {
        sampleFor(queue, true, 50 + i * 100, 90 + i * 100);
        sampleFor(queue, false, 100 + i * 100, 140 + i * 100);
    }
    TEST_ASSERT_TRUE(queue.read(2000).cancelled);
    TEST_ASSERT_FALSE(queue.read(2025).pressed);
}
void test_short_press_only_on_release() {
    EncoderPress button;
    TEST_ASSERT_TRUE(button.update(true, 10, 1) == Event::None);
    TEST_ASSERT_TRUE(button.update(true, 100, 1) == Event::None);
    TEST_ASSERT_TRUE(button.update(false, 110, 1) == Event::Click);
    TEST_ASSERT_TRUE(button.update(false, 120, 1) == Event::None);
}
void test_cancelled_gesture_resets_shared_handler() {
    EncoderPress button;
    button.update(true, 100, 1);
    button.reset();
    TEST_ASSERT_TRUE(button.update(false, 4000, 1) == Event::None);
    button.update(true, 4100, 1);
    TEST_ASSERT_TRUE(button.update(false, 4200, 1) == Event::Click);
}
void test_long_hold_suppresses_wake_on_release() {
    EncoderPress button;
    button.update(true, 100, 1);
    TEST_ASSERT_TRUE(button.update(true, 2099, 1) == Event::None);
    TEST_ASSERT_TRUE(button.update(true, 2100, 1) == Event::LongPress);
    TEST_ASSERT_TRUE(button.update(true, 4000, 2) == Event::None);
    TEST_ASSERT_TRUE(button.update(false, 4100, 2) == Event::None);
    button.update(true, 4200, 2);
    TEST_ASSERT_TRUE(button.update(false, 4300, 2) == Event::Click);
}
void test_screen_change_cancels_click_but_not_global_hold() {
    EncoderPress button;
    button.update(true, 10, 1);
    button.update(true, 20, 2);
    TEST_ASSERT_TRUE(button.update(false, 30, 1) == Event::None);
    button.update(true, 100, 1);
    TEST_ASSERT_TRUE(button.update(true, 2100, 2) == Event::LongPress);
}
void test_hold_wraparound_and_release_at_threshold() {
    EncoderPress button;
    button.update(true, UINT32_MAX - 1000, 1);
    TEST_ASSERT_TRUE(button.update(false, 999, 1) == Event::LongPress);
}
void test_page_actions() {
    TEST_ASSERT_TRUE(encoderClickAction(EncoderPage::Brew) == EncoderAction::BrewToggle);
    TEST_ASSERT_TRUE(encoderClickAction(EncoderPage::BrewStatus) == EncoderAction::BrewDismiss);
    TEST_ASSERT_TRUE(encoderClickAction(EncoderPage::Steam) == EncoderAction::None);
    TEST_ASSERT_TRUE(encoderClickAction(EncoderPage::Water) == EncoderAction::WaterToggle);
    TEST_ASSERT_TRUE(encoderClickAction(EncoderPage::Grind) == EncoderAction::GrindToggle);
    TEST_ASSERT_TRUE(encoderClickAction(EncoderPage::Standby) == EncoderAction::None);
    TEST_ASSERT_TRUE(encoderClickAction(EncoderPage::Other) == EncoderAction::None);
    TEST_ASSERT_TRUE(encoderDialAction(EncoderPage::Brew) == EncoderAction::Profile);
    TEST_ASSERT_TRUE(encoderDialAction(EncoderPage::Steam) == EncoderAction::Temperature);
    TEST_ASSERT_TRUE(encoderDialAction(EncoderPage::Water) == EncoderAction::Temperature);
    TEST_ASSERT_TRUE(encoderDialAction(EncoderPage::Grind) == EncoderAction::Weight);
    TEST_ASSERT_TRUE(encoderDialAction(EncoderPage::BrewStatus) == EncoderAction::None);
    TEST_ASSERT_TRUE(encoderDialAction(EncoderPage::Standby) == EncoderAction::None);
    TEST_ASSERT_TRUE(encoderDialAction(EncoderPage::Other) == EncoderAction::None);
}
void test_flush_hold_tracks_origin_and_survives_status_screen_release() {
    EncoderPress button;
    button.update(true, 100, 1);
    TEST_ASSERT_TRUE(button.update(true, 2100, 1) == Event::LongPress);
    TEST_ASSERT_EQUAL_INT(1, button.startScreen());
    TEST_ASSERT_TRUE(button.stayedOnScreen());
    TEST_ASSERT_TRUE(button.update(true, 3200, 2) == Event::None);
    TEST_ASSERT_FALSE(button.stayedOnScreen());
    TEST_ASSERT_TRUE(button.update(false, 3300, 2) == Event::None);
    // Returning to the original screen must not revive a cancelled gesture.
    button.update(true, 4000, 1);
    button.update(true, 4100, 2);
    TEST_ASSERT_TRUE(button.update(true, 6000, 1) == Event::LongPress);
    TEST_ASSERT_FALSE(button.stayedOnScreen());
}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_tap_between_ui_frames_is_preserved);
    RUN_TEST(test_queue_overflow_cancels_incomplete_gesture);
    RUN_TEST(test_short_press_only_on_release);
    RUN_TEST(test_cancelled_gesture_resets_shared_handler);
    RUN_TEST(test_long_hold_suppresses_wake_on_release);
    RUN_TEST(test_screen_change_cancels_click_but_not_global_hold);
    RUN_TEST(test_hold_wraparound_and_release_at_threshold);
    RUN_TEST(test_page_actions);
    RUN_TEST(test_flush_hold_tracks_origin_and_survives_status_screen_release);
    return UNITY_END();
}

#include <unity.h>
#include "display/drivers/common/EncoderInput.h"

void setUp() {}
void tearDown() {}

void test_full_detents_and_reverse() {
    EncoderRotation decoder;
    decoder.reset(3);
    TEST_ASSERT_EQUAL(0, decoder.update(2));
    TEST_ASSERT_EQUAL(0, decoder.update(0));
    TEST_ASSERT_EQUAL(0, decoder.update(1));
    TEST_ASSERT_EQUAL(1, decoder.update(3));
    TEST_ASSERT_EQUAL(0, decoder.update(1));
    TEST_ASSERT_EQUAL(0, decoder.update(0));
    TEST_ASSERT_EQUAL(0, decoder.update(2));
    TEST_ASSERT_EQUAL(-1, decoder.update(3));
}

void test_bounce_and_direction_reversal_cancel() {
    EncoderRotation decoder;
    decoder.reset(3);
    for (int i = 0; i < 10; ++i) {
        TEST_ASSERT_EQUAL(0, decoder.update(2));
        TEST_ASSERT_EQUAL(0, decoder.update(3));
    }
    TEST_ASSERT_EQUAL(0, decoder.update(2));
    TEST_ASSERT_EQUAL(0, decoder.update(0));
    TEST_ASSERT_EQUAL(0, decoder.update(2));
    TEST_ASSERT_EQUAL(0, decoder.update(3));
}

void test_invalid_transition_discards_partial_detent() {
    EncoderRotation decoder;
    decoder.reset(3);
    TEST_ASSERT_EQUAL(0, decoder.update(2));
    TEST_ASSERT_EQUAL(0, decoder.update(1)); // Both bits change.
    TEST_ASSERT_EQUAL(0, decoder.update(3));
    TEST_ASSERT_EQUAL(0, decoder.update(2));
    TEST_ASSERT_EQUAL(0, decoder.update(0));
    TEST_ASSERT_EQUAL(1, decoder.update(1));
}

void test_half_detents() {
    EncoderRotation decoder(2);
    decoder.reset(3);
    TEST_ASSERT_EQUAL(0, decoder.update(2));
    TEST_ASSERT_EQUAL(1, decoder.update(0));
    TEST_ASSERT_EQUAL(0, decoder.update(1));
    TEST_ASSERT_EQUAL(1, decoder.update(3));
}

void test_button_boot_hold_bounce_and_release() {
    EncoderButton button;
    TEST_ASSERT_FALSE(button.update(true, 1000));
    TEST_ASSERT_FALSE(button.update(true, 2000));
    TEST_ASSERT_FALSE(button.update(false, 2010));
    TEST_ASSERT_FALSE(button.update(false, 2040));
    TEST_ASSERT_FALSE(button.update(true, 2050));
    TEST_ASSERT_FALSE(button.update(false, 2055));
    TEST_ASSERT_FALSE(button.update(true, 2060));
    TEST_ASSERT_FALSE(button.update(true, 2089));
    TEST_ASSERT_TRUE(button.update(true, 2090));
    TEST_ASSERT_TRUE(button.update(false, 2100));
    TEST_ASSERT_TRUE(button.update(true, 2110));
    TEST_ASSERT_TRUE(button.update(false, 2120));
    TEST_ASSERT_FALSE(button.update(false, 2150));
}

void test_button_timer_wrap() {
    EncoderButton button;
    button.update(false, 0);
    button.update(false, 30);
    TEST_ASSERT_FALSE(button.update(true, UINT32_MAX - 10));
    TEST_ASSERT_FALSE(button.update(true, 10));
    TEST_ASSERT_TRUE(button.update(true, 20));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_full_detents_and_reverse);
    RUN_TEST(test_bounce_and_direction_reversal_cancel);
    RUN_TEST(test_invalid_transition_discards_partial_detent);
    RUN_TEST(test_half_detents);
    RUN_TEST(test_button_boot_hold_bounce_and_release);
    RUN_TEST(test_button_timer_wrap);
    return UNITY_END();
}

#include <unity.h>
#include "display/ui/default/PressFeedback.h"

void setUp() {}
void tearDown() {}
static void advance(uint32_t ms) { lv_tick_inc(ms); lv_anim_refr_now(); }
static int clicks;
static void clicked(lv_event_t *) { ++clicks; }

void test_press_and_release_fade_without_actions() {
    auto *screen = lv_obj_create(nullptr);
    auto *button = lv_imgbtn_create(screen);
    lv_obj_add_event_cb(button, clicked, LV_EVENT_CLICKED, nullptr);
    installPressFeedback(screen);
    installPressFeedback(screen);
    TEST_ASSERT_EQUAL(LV_OPA_COVER, lv_obj_get_style_opa(button, LV_PART_MAIN));
    lv_obj_add_state(button, LV_STATE_PRESSED);
    advance(80);
    TEST_ASSERT_EQUAL(LV_OPA_70, lv_obj_get_style_opa(button, LV_PART_MAIN));
    lv_obj_clear_state(button, LV_STATE_PRESSED);
    advance(150);
    TEST_ASSERT_EQUAL(LV_OPA_COVER, lv_obj_get_style_opa(button, LV_PART_MAIN));
    TEST_ASSERT_EQUAL(0, clicks);
    lv_obj_del(screen);
}

void test_encoder_pulse_and_delete_mid_animation() {
    auto *screen = lv_obj_create(nullptr);
    auto *button = lv_img_create(screen);
    lv_obj_add_event_cb(button, clicked, LV_EVENT_CLICKED, nullptr);
    pulsePressFeedback(button);
    advance(70);
    TEST_ASSERT_TRUE(lv_obj_has_state(button, LV_STATE_USER_1));
    TEST_ASSERT_FALSE(lv_obj_has_state(button, LV_STATE_PRESSED));
    TEST_ASSERT_EQUAL(LV_OPA_70, lv_obj_get_style_opa(button, LV_PART_MAIN));
    advance(50);
    advance(150);
    TEST_ASSERT_FALSE(lv_obj_has_state(button, LV_STATE_USER_1));
    TEST_ASSERT_EQUAL(LV_OPA_COVER, lv_obj_get_style_opa(button, LV_PART_MAIN));
    pulsePressFeedback(button);
    lv_obj_del(screen);
    advance(300); // Deleted objects must not leave callbacks referencing them.
    TEST_ASSERT_EQUAL(0, clicks);
}

int main() {
    lv_init();
    static lv_disp_drv_t display;
    lv_disp_drv_init(&display);
    display.hor_res = display.ver_res = 466;
    lv_disp_drv_register(&display);
    UNITY_BEGIN();
    RUN_TEST(test_press_and_release_fade_without_actions);
    RUN_TEST(test_encoder_pulse_and_delete_mid_animation);
    return UNITY_END();
}

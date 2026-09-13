#include <unity.h>
#include "display/ui/default/StartupFade.h"

void setUp() {}
void tearDown() {}

static void advance(uint32_t milliseconds) {
    lv_tick_inc(milliseconds);
    lv_anim_refr_now();
}

static void checkStartupTransition(uint32_t connectDelay) {
    lv_obj_t *standby = lv_scr_act();
    lv_obj_t *brew = lv_obj_create(nullptr);
    const auto layerChildren = lv_obj_get_child_cnt(lv_layer_top());
    startStartupFade(1000);
    advance(connectDelay);
    // Same immediate screen load used by EEZ when the controller connects.
    lv_scr_load_anim(brew, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
    TEST_ASSERT_EQUAL_PTR(brew, lv_scr_act());
    TEST_ASSERT_TRUE(lv_obj_is_valid(standby));
    TEST_ASSERT_NULL(lv_disp_get_default()->prev_scr);
    advance(1100);
    TEST_ASSERT_EQUAL_PTR(brew, lv_scr_act());
    TEST_ASSERT_EQUAL(layerChildren, lv_obj_get_child_cnt(lv_layer_top()));
    lv_scr_load(standby);
    lv_obj_del(brew);
}

void test_connect_before_first_frame() { checkStartupTransition(0); }
void test_connect_during_fade() { checkStartupTransition(400); }
void test_connect_after_fade() { checkStartupTransition(1200); }

int main() {
    lv_init();
    static lv_disp_drv_t display;
    lv_disp_drv_init(&display);
    display.hor_res = display.ver_res = 466;
    lv_disp_drv_register(&display);
    UNITY_BEGIN();
    RUN_TEST(test_connect_before_first_frame);
    RUN_TEST(test_connect_during_fade);
    RUN_TEST(test_connect_after_fade);
    return UNITY_END();
}

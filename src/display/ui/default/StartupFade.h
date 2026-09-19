#pragma once

#include <lvgl.h>

// Keep screen ownership with EEZ. Interrupting an auto-delete screen transition
// in LVGL 8 can delete the active screen when the controller connects early.
inline void startStartupFade(uint32_t duration) {
    lv_obj_t *overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);

    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, overlay);
    lv_anim_set_values(&animation, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_time(&animation, duration);
    lv_anim_set_exec_cb(&animation, [](void *obj, int32_t opacity) {
        lv_obj_set_style_bg_opa(static_cast<lv_obj_t *>(obj), opacity, 0);
    });
    lv_anim_set_ready_cb(&animation, [](lv_anim_t *anim) { lv_obj_del(static_cast<lv_obj_t *>(anim->var)); });
    lv_anim_start(&animation);
}

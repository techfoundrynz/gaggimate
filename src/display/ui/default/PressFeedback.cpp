#include "PressFeedback.h"

namespace {
lv_style_t releasedStyle;
lv_style_t pressedStyle;
lv_style_transition_dsc_t pressTransition;
lv_style_transition_dsc_t releaseTransition;
bool initialized = false;
const lv_style_prop_t properties[] = {LV_STYLE_OPA, LV_STYLE_PROP_INV};

void installedMarker(lv_event_t *) {}
void controlMarker(lv_event_t *) {}

void initStyles() {
    if (initialized) return;
    initialized = true;
    lv_style_transition_dsc_init(&pressTransition, properties, lv_anim_path_ease_out, 60, 0, nullptr);
    lv_style_transition_dsc_init(&releaseTransition, properties, lv_anim_path_ease_out, 120, 0, nullptr);
    lv_style_init(&releasedStyle);
    lv_style_set_opa(&releasedStyle, LV_OPA_COVER);
    lv_style_set_transition(&releasedStyle, &releaseTransition);
    lv_style_init(&pressedStyle);
    lv_style_set_opa(&pressedStyle, LV_OPA_70);
    lv_style_set_transition(&pressedStyle, &pressTransition);
}

void styleControl(lv_obj_t *obj) {
    if (lv_obj_get_event_user_data(obj, controlMarker)) return;
    lv_obj_add_style(obj, &releasedStyle, LV_PART_MAIN);
    lv_obj_add_style(obj, &pressedStyle, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_style(obj, &pressedStyle, LV_PART_MAIN | LV_STATE_USER_1);
    lv_obj_add_event_cb(obj, controlMarker, LV_EVENT_DELETE, &pressedStyle);
}

void styleControls(lv_obj_t *obj) {
    // Exclude screens, scrolling containers and gauges. Their children can still be controls.
    const bool control = lv_obj_has_flag(obj, LV_OBJ_FLAG_CLICKABLE) &&
        (lv_obj_check_type(obj, &lv_btn_class) || lv_obj_check_type(obj, &lv_imgbtn_class) ||
         lv_obj_check_type(obj, &lv_img_class) || lv_obj_check_type(obj, &lv_label_class));
    if (control) styleControl(obj);
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(obj); ++i)
        styleControls(lv_obj_get_child(obj, i));
}

void pulseTick(void *, int32_t) {}
void endPulse(lv_anim_t *anim) {
    lv_obj_clear_state(static_cast<lv_obj_t *>(anim->var), LV_STATE_USER_1);
}
}

void installPressFeedback(lv_obj_t *screen) {
    if (!screen || lv_obj_get_event_user_data(screen, installedMarker)) return;
    initStyles();
    styleControls(screen);
    // EEZ creates screens lazily; a recreated screen receives styles again.
    lv_obj_add_event_cb(screen, installedMarker, LV_EVENT_DELETE, &releasedStyle);
}

void pulsePressFeedback(lv_obj_t *control) {
    if (!control || lv_obj_has_flag(control, LV_OBJ_FLAG_HIDDEN) || lv_obj_has_state(control, LV_STATE_DISABLED)) return;
    initStyles();
    styleControl(control);
    // Separate visual state: never synthesize touch presses or action events.
    lv_anim_del(control, pulseTick);
    lv_obj_add_state(control, LV_STATE_USER_1);
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, control);
    lv_anim_set_exec_cb(&animation, pulseTick);
    lv_anim_set_values(&animation, 0, 1);
    lv_anim_set_time(&animation, 100);
    lv_anim_set_ready_cb(&animation, endPulse);
    lv_anim_start(&animation); // LVGL also deletes this animation if the object is deleted.
}

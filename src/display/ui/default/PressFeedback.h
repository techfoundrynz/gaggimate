#pragma once
#include <lvgl.h>

// Call after EEZ has created/updated the current screen. No generated files need editing.
void installPressFeedback(lv_obj_t *screen);
void pulsePressFeedback(lv_obj_t *control);
// Held feedback, mirroring how touch holds LV_STATE_PRESSED for the whole press.
void setPressFeedback(lv_obj_t *control, bool pressed);

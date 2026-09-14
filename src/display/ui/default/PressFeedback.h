#pragma once
#include <lvgl.h>

// Call after EEZ has created/updated the current screen. No generated files need editing.
void installPressFeedback(lv_obj_t *screen);
void pulsePressFeedback(lv_obj_t *control);

#include "main.h"
#include <RemoteConsole.h>

#ifndef GAGGIMATE_HEADLESS
#include <lvgl.h>
#endif

Controller controller;

void setup() {
    Serial.begin(115200);
    remote_console::begin();
    controller.setup();
}

void loop() {
    controller.loop();
    delay(50);
}

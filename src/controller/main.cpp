#include "main.h"
#include "ControllerConfig.h"
#include "GaggiMateController.h"
#include <RemoteConsole.h>

GaggiMateController controller(BUILD_GIT_VERSION);

void setup() {
    Serial.begin(115200);
    remote_console::begin();
    controller.setup();
}

void loop() { controller.loop(); }

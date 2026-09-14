#pragma once

#include <cmath>
#include <cstdlib>
#include <display/core/Controller.h>
#include <display/plugins/HardwareScalePlugin.h>

// Simulator-only controls model physically placing/removing a calibration mass.
inline String simulateHardwareScaleLoad(Controller *controller, AsyncWebServerRequest *request) {
    if (!HardwareScales.isEnabled())
        return "Enable Hardware Scales in Plugins, then save and restart";
    const String input = request->arg("mass");
    char *end = nullptr;
    const double mass = std::strtod(input.c_str(), &end);
    const String position = request->arg("position");
    if (end == input.c_str() || *end != '\0' || !std::isfinite(mass) || mass < 0 || mass > 500)
        return "Use a simulated load between 0 and 500 grams";
    if (position != "left" && position != "centre" && position != "right")
        return "Choose left, centre or right";
    controller->getClientController()->simulateScaleLoad(mass, position == "left" ? 0.8 : position == "right" ? 0.2 : 0.5);
    return "";
}

inline void hardwareScaleSimulationStatus(Controller *controller, JsonDocument &doc) {
    auto *client = controller->getClientController();
    doc["simulated"] = true;
    doc["simMass"] = client->simulatedScaleMass();
    const double share = client->simulatedScaleLeftShare();
    doc["simPosition"] = share > 0.5 ? "left" : share < 0.5 ? "right" : "centre";
}

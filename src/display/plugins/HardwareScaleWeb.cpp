#include "HardwareScalePlugin.h"
#include "WebUIPlugin.h"
#ifdef GAGGIMATE_SIM
#include "HardwareScaleSimulation.h"
#endif

void WebUIPlugin::handleHardwareScale(AsyncWebServerRequest *request) {
    JsonDocument doc(&psramAllocator);
    String error;
    if (request->method() == HTTP_POST) {
#ifdef GAGGIMATE_SIM
        if (request->arg("action") == "simulate")
            error = simulateHardwareScaleLoad(controller, request);
        else
#endif
        error = HardwareScales.command(request->arg("action"), request->arg("mass").toDouble());
    }
    HardwareScales.writeStatus(doc);
#ifdef GAGGIMATE_SIM
    hardwareScaleSimulationStatus(controller, doc);
#endif
    if (error.length())
        doc["error"] = error;
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    response->setCode(error.length() ? 400 : 200);
    response->addHeader("Cache-Control", "no-store");
    serializeJson(doc, *response);
    request->send(response);
}

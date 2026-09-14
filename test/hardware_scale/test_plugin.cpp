#include <cassert>
#include <cmath>
#include <cstdio>
#include "../../src/display/plugins/HardwareScalePlugin.cpp"

static unsigned long now = 100;
unsigned long millis() { return now; }

static void samples(Controller &controller, int32_t left, int32_t right) {
    for (int i = 0; i < 12; ++i) {
        now += 100;
        controller.client.receive(true, left, right, false);
    }
}

int main() {
    Controller controller;
    PluginManager manager;
    HardwareScalePlugin scale;
    assert(!scale.isEnabled() && !scale.isReady());
    assert(!scale.command("tare").empty()); // Plugin not registered: no hardware operations.
    scale.setup(&controller, &manager);
    controller.tareSelected = [&] { scale.tare(); };
    manager.trigger("controller:bluetooth:connect");
    assert(controller.client.enabled);
    assert(controller.client.clock == 17 && controller.client.left == 18 && controller.client.right == 39);
    samples(controller, 100000, -200000);
    assert(!scale.isReady()); // Raw presence is not calibrated weight.
    assert(scale.command("empty").empty());
    assert(!scale.command("right").empty()); // Wrong order.
    samples(controller, 108000, -204000);
    assert(scale.command("left", 100).empty());
    samples(controller, 102000, -216000);
    assert(scale.command("right").empty());
    assert(scale.isReady());
    samples(controller, 105000, -210000);
    assert(std::abs(controller.weights.back() - 100) < 1e-6);
    scale.tare(); // source/event routing is covered by the controller scale tests
    samples(controller, 105000, -210000);
    assert(std::abs(controller.weights.back()) < 1e-6);
    samples(controller, 106000, -212000);
    assert(std::abs(controller.weights.back() - 20) < 1e-6);
    scale.tare();
    samples(controller, 106000, -212000);
    assert(std::abs(controller.weights.back()) < 1e-6);
    controller.active = true;
    assert(!scale.command("disable").empty());
    assert(!scale.command("empty").empty());
    controller.active = false;
    controller.updating = true;
    assert(!scale.command("empty").empty());
    controller.updating = false;
    now += 1001;
    assert(!scale.isReady());
    assert(!scale.command("tare").empty());
    samples(controller, 105000, -210000);
    controller.client.receive(false, 0, 0, false);
    assert(!scale.isReady());
    const auto measurements = controller.weights.size();
    manager.trigger("controller:bluetooth:disconnect");
    assert(controller.weights.size() == measurements);

    // Calibration is one persistent record; a new instance restores the empty-tray zero,
    // not the transient cup tare. Failed writes preserve the previous calibration.
    Controller rebooted;
    PluginManager rebootManager;
    HardwareScalePlugin restored;
    restored.setup(&rebooted, &rebootManager);
    rebootManager.trigger("controller:bluetooth:connect");
    assert(rebooted.client.enabled);
    samples(rebooted, 105000, -210000);
    assert(std::abs(rebooted.weights.back() - 100) < 1e-6);
    Preferences::failWrites = true;
    samples(rebooted, 100000, -200000);
    assert(restored.command("empty").empty());
    samples(rebooted, 108000, -204000);
    assert(restored.command("left", 100).empty());
    samples(rebooted, 102000, -216000);
    assert(!restored.command("right").empty());
    assert(restored.command("cancel").empty());
    Preferences::failWrites = false;
    assert(restored.command("empty").empty());
    assert(!restored.isReady()); // Calibration suppresses measured output.
    const auto beforeCalibration = rebooted.weights.size();
    samples(rebooted, 105000, -210000);
    assert(rebooted.weights.size() == beforeCalibration);
    assert(restored.command("cancel").empty());
    assert(restored.isReady());
    rebootManager.trigger("controller:bluetooth:disconnect");
    assert(!restored.isReady());
    rebooted.client.receive(false, 0, 0, true);
    JsonDocument status;
    restored.writeStatus(status);
    assert(status["configError"].as<bool>());
    Controller newPins;
    PluginManager newManager;
    HardwareScalePlugin moved;
    newPins.settings.clock = 12;
    moved.setup(&newPins, &newManager);
    newManager.trigger("controller:bluetooth:connect");
    assert(newPins.client.clock == 12);
    samples(newPins, 105000, -210000);
    assert(!moved.isReady()); // Old calibration cannot silently follow changed wiring.
    puts("Wired scale enable, calibration, persistence, tare, stale data and disconnect checks passed");
}

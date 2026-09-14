#include <display/core/Controller.h>
#include "../../src/display/core/ControllerScale.cpp"
#include <cassert>
#include <climits>
#include <limits>
#include <cstdio>

static unsigned long now = 0;
unsigned long millis() { return now; }

int main() {
    for (auto source : {ScaleSource::Bluetooth, ScaleSource::Wired}) {
        BLEScales = {};
        HardwareScales = {};
        now = 0;
        Controller controller;
        controller.settings.wired = source == ScaleSource::Wired;
        controller.setupScale();
        auto other = source == ScaleSource::Wired ? ScaleSource::Bluetooth : ScaleSource::Wired;
        assert(controller.getScaleStatus().connected && !controller.isScaleHealthy());
        assert(!controller.onScaleMeasurement(other, 900));
        assert(controller.weights.empty());
        assert(!controller.onScaleMeasurement(source, std::numeric_limits<double>::quiet_NaN()));
        assert(!controller.onScaleMeasurement(source, 10001));
        assert(!controller.isScaleHealthy());
        assert(controller.onScaleMeasurement(source, 12.5)); // a valid reading at millis()==0 counts
        assert(controller.isScaleHealthy() && controller.weights.back() == 12.5);
        assert(controller.getScaleStatus().batteryPercent == (source == ScaleSource::Bluetooth ? 50 : -1));
        BLEScales.battery = 255;
        assert(controller.getScaleStatus().batteryPercent == -1);
        BLEScales.battery = 50;
        controller.settings.wired = !controller.settings.wired;
        assert(controller.isScaleSelected(source)); // saved preference changes require restart
        controller.tareScale();
        controller.manager.trigger("controller:brew:prestart");
        controller.manager.trigger("controller:grind:start");
        controller.manager.trigger("controller:brew:end");
        assert(controller.comms.tares == 1);
        assert(BLEScales.tares == (source == ScaleSource::Bluetooth ? 3 : 0));
        assert(HardwareScales.tares == (source == ScaleSource::Wired ? 3 : 0));
        assert(BLEScales.timerStops == (source == ScaleSource::Bluetooth ? 1 : 0));

        now = 1500;
        assert(!controller.onScaleMeasurement(other, 20)); // unselected packets cannot keep it healthy
        assert(!controller.isScaleHealthy());
        BrewProcess brew;
        controller.currentProcess = &brew;
        controller.onScaleUnavailable();
        assert(controller.stops == 1 && controller.dispatched == 1);
        brew.target = ProcessTarget::TIME;
        controller.onScaleUnavailable();
        assert(controller.stops == 1);
        GrindProcess grind;
        controller.currentProcess = &grind;
        controller.onScaleUnavailable();
        assert(controller.stops == 2);
        controller.currentVolumetricSource = VolumetricMeasurementSource::FLOW_ESTIMATION;
        controller.onScaleUnavailable();
        assert(controller.stops == 2);
        controller.currentVolumetricSource = VolumetricMeasurementSource::SCALE;
        controller.onScaleMeasurement(source, 22);
        controller.onScaleUnavailable();
        assert(controller.stops == 2); // recovered before the stop check
        controller.onScaleDisconnected(other);
        assert(controller.isScaleHealthy());
        controller.onScaleDisconnected(source);
        assert(!controller.isScaleHealthy());
        controller.onScaleMeasurement(source, 23);
        assert(controller.isScaleHealthy());
        if (source == ScaleSource::Bluetooth) BLEScales.connected = false;
        else HardwareScales.status.connected = HardwareScales.status.ready = false;
        assert(!controller.getScaleStatus().connected && !controller.isScaleHealthy());
        controller.onScaleUnavailable();
        assert(controller.stops == 3);
        BLEScales.connected = true;
        HardwareScales.status.connected = HardwareScales.status.ready = true;
        now = ULONG_MAX - 10;
        controller.onScaleMeasurement(source, 24);
        now = 20;
        assert(controller.isScaleHealthy()); // millis wraparound
    }
    Controller controller;
    controller.settings.wired = true;
    controller.setupScale();
    HardwareScales.status.ready = false; // paired ADCs, calibration not yet usable
    controller.onScaleMeasurement(ScaleSource::Wired, 1);
    assert(controller.getScaleStatus().connected && !controller.isScaleHealthy());
    puts("Shared scale selection, freshness, metadata, tare and process-stop checks passed");
}

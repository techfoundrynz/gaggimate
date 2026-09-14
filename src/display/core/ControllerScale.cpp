#include "Controller.h"
#include <display/core/process/BrewProcess.h>
#include <display/core/process/GrindProcess.h>
#include <display/plugins/HardwareScalePlugin.h>
#include <display/plugins/BLEScalePlugin.h>
#include <cmath>

void Controller::setupScale() {
    scaleSource = settings.isHardwareScaleActive() ? ScaleSource::Wired : ScaleSource::Bluetooth;
    pluginManager->on("controller:brew:prestart", [this](const Event &) { tareSelectedScale(); });
    pluginManager->on("controller:grind:start", [this](const Event &) { tareSelectedScale(); });
    pluginManager->on("controller:brew:end", [this](const Event &) {
        if (isScaleSelected(ScaleSource::Bluetooth)) BLEScales.stopTimer();
    });
}

gm::ScaleStatus Controller::getScaleStatus() const {
    gm::ScaleStatus status;
    if (isScaleSelected(ScaleSource::Wired)) {
        status = HardwareScales.getScaleStatus();
    } else {
        status.connected = BLEScales.isConnected();
        status.configured = status.connected || settings.getSavedScale() != "";
        status.ready = status.connected;
        if (status.connected && BLEScales.hasBatteryLevel()) {
            const int battery = BLEScales.getBatteryLevel();
            if (battery >= 0 && battery <= 100) status.batteryPercent = battery;
        }
    }
    std::lock_guard<std::mutex> lock(scaleMutex);
    status.ready = status.ready && scaleMeasurementReceived && millis() - lastScaleMeasurement < SCALE_GRACE_PERIOD_MS;
    return status;
}

bool Controller::onScaleMeasurement(ScaleSource source, double measurement) {
    if (!isScaleSelected(source) || !std::isfinite(measurement) || measurement < -1000 || measurement > 10000) return false;
    {
        std::lock_guard<std::mutex> lock(scaleMutex);
        scaleMeasurementReceived = true;
        lastScaleMeasurement = millis();
    }
    onVolumetricMeasurement(measurement, VolumetricMeasurementSource::SCALE);
    return true;
}

void Controller::tareSelectedScale() {
    if (isScaleSelected(ScaleSource::Wired)) HardwareScales.tare();
    else BLEScales.tare();
}

void Controller::onScaleDisconnected(ScaleSource source) {
    if (!isScaleSelected(source)) return;
    std::lock_guard<std::mutex> lock(scaleMutex);
    scaleMeasurementReceived = false;
}

void Controller::tareScale() {
    comms.tare(); // controller-side flow estimate has a separate zero
    tareSelectedScale();
}

bool Controller::isScaleHealthy() const {
    return getScaleStatus().ready;
}

void Controller::onScaleUnavailable() {
    std::vector<const char *> events;
    {
        std::lock_guard<std::recursive_mutex> lock(processMutex);
        if (isScaleHealthy()) return; // a new reading may have arrived before acquiring the process lock
        if (!currentProcess || currentVolumetricSource != VolumetricMeasurementSource::SCALE) return;
        bool usesWeight = false;
        if (currentProcess->getType() == MODE_BREW)
            usesWeight = static_cast<BrewProcess *>(currentProcess)->target == ProcessTarget::VOLUMETRIC;
        if (currentProcess->getType() == MODE_GRIND)
            usesWeight = static_cast<GrindProcess *>(currentProcess)->target == ProcessTarget::VOLUMETRIC;
        if (!usesWeight) return;
        ESP_LOGW("Scales", "Scale unavailable; stopping weight-target process");
        deactivateLocked(events);
    }
    dispatchEvents(events);
}

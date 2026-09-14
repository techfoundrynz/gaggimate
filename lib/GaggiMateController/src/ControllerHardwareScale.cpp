#include "GaggiMateController.h"
#include <peripherals/HardwareScalePins.h>

void GaggiMateController::setupHardwareScales() {
    _comms.onHardwareScaleControl([this](bool enabled, uint32_t clock, uint32_t left, uint32_t right) {
        const bool valid = !enabled || hardwareScalePinsAvailable(_config, clock, left, right, gearpumpAddon != nullptr);
        hardwareScaleConfigError.store(!valid);
        if (!valid) {
            if (hardwareScale) hardwareScale->configure(false, 0, 0, 0);
            ESP_LOGE(LOG_TAG, "Rejected HX711 pins: clock=%u left=%u right=%u", clock, left, right);
            _comms.sendHardwareScale(false, 0, 0, true);
            return;
        }
        if (!hardwareScale && enabled) {
            hardwareScale = new DualHx711(clock, left, right, [this](bool available, int32_t a, int32_t b) {
                const bool error = hardwareScaleConfigError.load();
                _comms.sendHardwareScale(available && !error, a, b, error);
            });
            if (!hardwareScale->setup()) {
                delete hardwareScale;
                hardwareScale = nullptr;
                ESP_LOGE(LOG_TAG, "Unable to start dual HX711 reader");
                _comms.sendHardwareScale(false, 0, 0);
                return;
            }
        }
        if (hardwareScale) hardwareScale->configure(enabled, clock, left, right);
    });
}

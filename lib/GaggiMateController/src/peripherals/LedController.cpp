#include "LedController.h"

LedController::LedController(SoftWire *i2c, std::recursive_mutex &busMutex)
    : ncp5623(i2c), busMutex(busMutex) {
    this->pca9634 = new PCA9634(0x00, i2c);
}

void LedController::setup() {
    this->initialize();
    this->disable();
}

bool LedController::isAvailable() { return this->initialize(); }

void LedController::setChannel(uint8_t channel, uint8_t brightness) {
    std::lock_guard<std::recursive_mutex> guard(busMutex);
    if (!initialized || channel >= 8)
        return;
    if (useNcp5623) {
        // ToFnLED has RGB only; channels 4..7 belong to Alba's external lights.
        if (channel >= 4)
            return;
        rgbw[channel] = brightness;
        if (!ncp5623.setRgbw(rgbw[0], rgbw[1], rgbw[2], rgbw[3]))
            ESP_LOGE("LedController", "Error updating ToFnLED RGB");
        return;
    }
    ESP_LOGI("LedController", "Setting channel %u to %u", channel, brightness);
    uint8_t error = this->pca9634->write1(channel, brightness);
    if (error > 0) {
        ESP_LOGE("LedController", "Error setting channel %u to %u: %d", channel, brightness, this->pca9634->lastError());
    }
}

void LedController::disable() {
    std::lock_guard<std::recursive_mutex> guard(busMutex);
    if (!initialized)
        return;
    if (useNcp5623) {
        for (auto &value : rgbw)
            value = 0;
        if (!ncp5623.setRgbw(0, 0, 0, 0))
            ESP_LOGE("LedController", "Error disabling ToFnLED RGB");
        return;
    }
    this->pca9634->allOff();
    this->pca9634->write1(4, 0xFF);
    this->pca9634->write1(5, 0xFF);
}

bool LedController::initialize() {
    std::lock_guard<std::recursive_mutex> guard(busMutex);
    if (this->initialized) {
        return true;
    }
    bool retval = this->pca9634->begin();
    if (!retval) {
        if (ncp5623.begin()) {
            useNcp5623 = true;
            initialized = true;
            ESP_LOGI("LedController", "Initialized ToFnLED NCP5623 at 0x38");
            return true;
        }
        ESP_LOGI("LedController", "No Alba or ToFnLED LED driver detected");
        return false;
    }
    ESP_LOGI("LedController", "Initialized PCA9634");
    this->initialized = retval;
    this->pca9634->setMode1(PCA963X_MODE1_NONE);
    this->pca9634->setMode2(PCA963X_MODE2_TOTEMPOLE);
    this->pca9634->allOff();
    this->pca9634->write1(4, 0xFF);
    this->pca9634->write1(5, 0xFF);
    this->pca9634->setLedDriverModeAll(PCA963X_LEDPWM);
    ESP_LOGI("LedController", "Mode1: %d", this->pca9634->getMode1());
    ESP_LOGI("LedController", "Mode2: %d", this->pca9634->getMode2());
    return retval;
}

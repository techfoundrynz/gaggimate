#include "LedController.h"

LedController::LedController(SoftWireBus *bus) : bus(bus), ncp5623(bus->wire()) {
    this->pca9634 = new PCA9634(0x00, bus->wire());
}

void LedController::setup() {
    this->initialize();
    this->disable();
}

bool LedController::isAvailable() { return this->initialize(); }

void LedController::setChannel(uint8_t channel, uint8_t brightness) {
    ESP_LOGI("LedController", "Setting channel %u to %u", channel, brightness);
    SoftWireBus::Guard guard(bus);
    if (channel < CHANNEL_COUNT) {
        channels[channel] = brightness;
    }
    if (!guard) {
        ESP_LOGE("LedController", "Bus busy, channel %u deferred to next health check", channel);
        healthy = false;
        return;
    }
    if (driver == Driver::Ncp5623) {
        if (channel >= 4) {
            return; // The NCP5623 has no channels past R, G, B and the mixed white.
        }
        if (!ncp5623.setRgbw(channels[0], channels[1], channels[2], channels[3])) {
            ESP_LOGE("LedController", "Error updating NCP5623 RGB");
            healthy = false;
        }
        return;
    }
    uint8_t error = this->pca9634->write1(channel, brightness);
    if (error > 0) {
        ESP_LOGE("LedController", "Error setting channel %u to %u: %d", channel, brightness, this->pca9634->lastError());
        this->recover();
    }
}

void LedController::disable() {
    SoftWireBus::Guard guard(bus);
    if (driver == Driver::Ncp5623) {
        memset(channels, 0, sizeof(channels));
        if (!guard || !ncp5623.setRgbw(channels[0], channels[1], channels[2], channels[3])) {
            healthy = false;
        }
        return;
    }
    const uint8_t off[CHANNEL_COUNT] = {0, 0, 0, 0, 0xFF, 0xFF, 0, 0};
    memcpy(channels, off, sizeof(channels));
    if (!guard || this->pca9634->writeAll(channels) != PCA963X_OK) {
        healthy = false;
    }
}

void LedController::healthCheck() {
    SoftWireBus::Guard guard(bus);
    if (!guard) {
        return;
    }
    // The NCP5623 has no readable mode register, so re-apply the last commanded
    // colour instead; without this a write deferred by a busy bus never lands.
    if (driver == Driver::Ncp5623) {
        if (!healthy) {
            healthy = ncp5623.setRgbw(channels[0], channels[1], channels[2], channels[3]);
        }
        return;
    }
    uint8_t mode1 = this->pca9634->getMode1();
    bool ok = this->pca9634->lastError() == PCA963X_OK && (mode1 & PCA963X_MODE1_SLEEP) == 0;
    if (ok && healthy) {
        return;
    }
    if (healthy) {
        ESP_LOGW("LedController", "PCA9634 health check failed (mode1=0x%02X), restarting", mode1);
    }
    if (this->recover()) {
        ESP_LOGI("LedController", "PCA9634 recovered");
    }
}

// Caller must hold the bus lock.
bool LedController::recover() {
    bus->clear();
    driver = Driver::None;
    healthy = this->initialize();
    return healthy;
}

bool LedController::initialize() {
    if (driver != Driver::None) {
        return true;
    }
    SoftWireBus::Guard guard(bus);
    if (!guard) {
        return false;
    }
    bool retval = this->pca9634->begin();
    if (!retval) {
        if (ncp5623.begin()) {
            driver = Driver::Ncp5623;
            ESP_LOGI("LedController", "Initialized NCP5623 at 0x38");
            return true;
        }
        if (healthy) {
            ESP_LOGE("LedController", "No PCA9634 or NCP5623 LED driver detected");
        }
        return false;
    }
    ESP_LOGI("LedController", "Initialized PCA9634");
    this->pca9634->setMode1(PCA963X_MODE1_NONE);
    this->pca9634->setMode2(PCA963X_MODE2_TOTEMPOLE);
    // Restores the last commanded state; on first boot this is the "off" pattern.
    this->pca9634->writeAll(channels);
    retval = this->pca9634->setLedDriverModeAll(PCA963X_LEDPWM) == PCA963X_OK;
    driver = retval ? Driver::Pca9634 : Driver::None;
    ESP_LOGI("LedController", "Mode1: %d", this->pca9634->getMode1());
    ESP_LOGI("LedController", "Mode2: %d", this->pca9634->getMode2());
    return retval;
}

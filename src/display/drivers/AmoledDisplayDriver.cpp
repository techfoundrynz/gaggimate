#include "AmoledDisplayDriver.h"
#include "AmoledDisplay/pin_config.h"
#include <Wire.h>
#include <cstring>
#include <display/drivers/common/LV_Helper.h>

AmoledDisplayDriver *AmoledDisplayDriver::instance = nullptr;

static bool detectI2CDevice(uint8_t address, const char *deviceName = nullptr) {
    for (uint8_t retry = 0; retry < 5; retry++) {
        Wire.beginTransmission(address);
        if (Wire.endTransmission() == 0) {
            if (deviceName) {
                ESP_LOGI("AmoledDisplayDriver", "Found %s at 0x%02X\n", deviceName, address);
            } else {
                ESP_LOGI("AmoledDisplayDriver", "Found device at 0x%02X\n", address);
            }
            return true;
        }
        delay(100);
    }
    return false;
}

// Variant indices are persisted in NVS (GM-140) — only append, never reorder
static constexpr AmoledHwConfig VARIANTS[] = {LILYGO_T_DISPLAY_S3_DS_HW_CONFIG, WAVESHARE_S3_TOUCH_AMOLED_1_43_HW_CONFIG,
                                              WAVESHARE_S3_AMOLED_HW_CONFIG, VIEWE_1_5_HW_CONFIG};
static constexpr const char *VARIANT_NAMES[] = {"LilyGo T-Display", "Waveshare 1.43\" AMOLED Display",
                                                "Waveshare AMOLED Display", "VIEWE 1.5\" AMOLED Knob Display"};
static constexpr int VARIANT_COUNT = sizeof(VARIANTS) / sizeof(VARIANTS[0]);

bool AmoledDisplayDriver::isCompatible() {
    for (int i = 0; i < VARIANT_COUNT; i++) {
        ESP_LOGI("AmoledDisplayDriver", "Testing %s...", VARIANT_NAMES[i]);
        if (testHw(VARIANTS[i])) {
            hwConfig = VARIANTS[i];
            variant = i;
            return true;
        }
    }
    return false;
}

bool AmoledDisplayDriver::selectVariant(int variant) {
    if (variant < 0 || variant >= VARIANT_COUNT)
        return false;
    hwConfig = VARIANTS[variant];
    this->variant = variant;
    return true;
}

void AmoledDisplayDriver::init() {
    panel = new Amoled_DisplayPanel(hwConfig);
    ESP_LOGI("AmoledDisplayDriver", "Initializing AMOLED display...");

    if (!panel->begin()) {
        for (uint8_t i = 0; i < 20; i++) {
            ESP_LOGE("AmoledDisplayDriver", "Error, failed to initialize AMOLED display");
            delay(1000);
        }
        ESP.restart();
    }

    beginLvglHelper(*panel);
    beginEncoder(hwConfig.encoder);
}

bool AmoledDisplayDriver::supportsSDCard() { return hwConfig.sd_cs != -1; }

bool AmoledDisplayDriver::installSDCard() { return panel->installSD(); }

// The CST816 family shares one I2C address, so an ACK alone would also match a
// sibling part; confirm the model through SensorLib's chip identification.
static bool detectCST820(const AmoledHwConfig &hwConfig) {
    TouchClassCST816 touch;
    touch.setPins(hwConfig.tp_rst, hwConfig.tp_int);
    return touch.begin(Wire, CST816_SLAVE_ADDRESS, hwConfig.i2c_sda, hwConfig.i2c_scl) &&
           std::strcmp(touch.getModelName(), "CST820") == 0;
}

bool AmoledDisplayDriver::testHw(AmoledHwConfig hwConfig) {
    // No Wire on these pins, definitely wrong board
    if (!Wire.begin(hwConfig.i2c_sda, hwConfig.i2c_scl))
        return false;

    // Required: PCF8563 (RTC) when present, and a touch sensor
    // Touch sensor: CST92XX (1.75 inch), FT3168 (1.43 inch) or CST820 (1.5 inch knob)
    // Some boards (e.g. Waveshare 1.43") have no PCF8563 RTC; skip that check for them
    bool pcf8563Found = (hwConfig.pcf8563_int == -1) || detectI2CDevice(PCF8563_DEVICE_ADDRESS, "PCF8563 RTC");

    bool touchFound = detectI2CDevice(CST92XX_DEVICE_ADDRESS, "CST92XX Touch Sensor") ||
                      detectI2CDevice(FT3168_DEVICE_ADDRESS, "FT3168 Touch Sensor") || detectCST820(hwConfig);

    Wire.end();
    return pcf8563Found && touchFound;
}

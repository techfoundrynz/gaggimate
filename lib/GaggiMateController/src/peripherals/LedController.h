#ifndef LEDCONTROLLER_H
#define LEDCONTROLLER_H

#include <Arduino.h>
#include <PCA9634/PCA9634.h>
#include <SoftWire.h>
#include <mutex>
#include "Ncp5623.h"

class LedController {
  public:
    LedController(SoftWire *i2c, std::recursive_mutex &busMutex);
    void setup();
    bool isAvailable();
    void setChannel(uint8_t channel, uint8_t brightness);
    void disable();

  private:
    bool initialize();

    PCA9634 *pca9634 = nullptr;
    Ncp5623 ncp5623;
    std::recursive_mutex &busMutex;
    bool useNcp5623 = false;
    uint8_t rgbw[4] = {};
    bool initialized = false;
};

#endif // LEDCONTROLLER_H

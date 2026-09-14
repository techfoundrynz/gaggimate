#pragma once
#include <cstdint>
#include "../ControllerConfig.h"

inline bool hardwareScalePinsAvailable(const ControllerConfig &board, uint32_t clock, uint32_t left,
                                       uint32_t right, bool gearpump) {
    if (clock == left || clock == right || left == right) return false;
    for (uint32_t pin : {clock, left, right}) {
        // ESP32-S3: avoid nonexistent GPIOs, flash/PSRAM, USB and strapping pins.
        if (pin > 48 || (pin >= 22 && pin <= 37) || pin == 0 || pin == 3 || pin == 19 || pin == 20 || pin == 45 || pin == 46)
            return false;
        for (uint32_t used : {uint32_t(board.heaterPin), uint32_t(board.pumpPin), uint32_t(board.valvePin),
                              uint32_t(board.altPin), uint32_t(board.maxSckPin), uint32_t(board.maxCsPin),
                              uint32_t(board.maxMisoPin), uint32_t(board.brewButtonPin), uint32_t(board.steamButtonPin),
                              uint32_t(board.ext2Pin), uint32_t(board.ext3Pin), 11u, 40u})
            if (pin == used) return false;
        if (board.capabilites.dimming && pin == board.pumpSensePin) return false;
        if (board.capabilites.pressure && (pin == board.pressureScl || pin == board.pressureSda)) return false;
        if (board.sunriseSclPin != board.sunriseSdaPin &&
            (pin == board.sunriseSclPin || pin == board.sunriseSdaPin)) return false;
        if (gearpump && pin == board.ext1Pin) return false;
    }
    return true;
}

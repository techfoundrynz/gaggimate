#pragma once

#include <stdint.h>

// Release the bus and recover interrupted transactions before probing accessories.
bool prepareAccessoryBus(uint8_t sda, uint8_t scl);

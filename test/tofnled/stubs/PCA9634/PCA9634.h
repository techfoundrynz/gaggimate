#pragma once
#include <SoftWire.h>
#include <utility>
#define PCA963X_MODE1_NONE 0
#define PCA963X_MODE2_TOTEMPOLE 4
#define PCA963X_LEDPWM 2
class PCA9634 {
  public:
    inline static bool present = false;
    inline static int beginCalls = 0;
    inline static int mode1 = -1, mode2 = -1, driverMode = -1;
    inline static std::vector<std::pair<int, int>> writes;
    PCA9634(int, SoftWire *) {}
    bool begin() { ++beginCalls; return present; }
    int write1(int channel, int value) { writes.emplace_back(channel, value); return 0; }
    void allOff() { writes.emplace_back(-1, 0); }
    void setMode1(int value) { mode1 = value; }
    void setMode2(int value) { mode2 = value; }
    void setLedDriverModeAll(int value) { driverMode = value; }
    int lastError() { return 0; }
};

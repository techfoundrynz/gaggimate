#pragma once
struct TestBLEScale {
    bool connected = true;
    int battery = 50, tares = 0, timerStops = 0;
    bool isConnected() const { return connected; }
    bool hasBatteryLevel() const { return battery >= 0; }
    int getBatteryLevel() const { return battery; }
    void tare() { ++tares; }
    void stopTimer() { ++timerStops; }
};
inline TestBLEScale BLEScales;

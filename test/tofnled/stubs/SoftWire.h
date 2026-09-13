#pragma once
#include <cstdint>
#include <vector>
class SoftWire {
  public:
    bool present = true;
    int failAt = -1;
    std::vector<std::vector<uint8_t>> frames;
    void beginTransmission(uint8_t address) { frames.push_back({address}); }
    void write(uint8_t value) { frames.back().push_back(value); }
    int endTransmission() { return !present || static_cast<int>(frames.size()) == failAt ? 2 : 0; }
};

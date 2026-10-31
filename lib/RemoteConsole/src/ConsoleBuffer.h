#pragma once

#include <array>
#include <cstdint>
#include <cstring>

namespace remote_console {
constexpr size_t TEXT_SIZE = 161;
constexpr uint32_t LEASE_MS = 15000;
constexpr uint32_t RENEW_MS = 5000;

struct Record {
    uint32_t timestamp = 0;
    uint32_t sequence = 0;
    uint32_t dropped = 0;
    bool controller = false;
    char text[TEXT_SIZE]{};
};

// Caller supplies synchronization. Overload keeps recent lines, with explicit loss accounting.
template <size_t Capacity> class Buffer {
  public:
    void clear() {
        head = count = 0;
        dropped = 0;
    }
    void push(const Record &record) {
        if (count == Capacity) {
            dropped += records[head].dropped + 1;
            head = (head + 1) % Capacity;
            --count;
        }
        records[(head + count++) % Capacity] = record;
    }
    bool pop(Record &record) {
        if (!count)
            return false;
        record = records[head];
        head = (head + 1) % Capacity;
        --count;
        record.dropped += dropped;
        dropped = 0;
        return true;
    }

  private:
    std::array<Record, Capacity> records{};
    size_t head = 0;
    size_t count = 0;
    uint32_t dropped = 0;
};

inline bool leaseActive(uint32_t now, uint32_t renewed) { return uint32_t(now - renewed) < LEASE_MS; }
} // namespace remote_console

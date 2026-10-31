#pragma once

#include "ConsoleBuffer.h"
#include <map>
#include <vector>

namespace remote_console {
// Caller supplies synchronization; leases cover tabs that vanish without a clean close.
class Observers {
  public:
    void subscribe(uint32_t client, bool enabled, uint32_t now) {
        if (enabled)
            renewed[client] = now;
        else
            renewed.erase(client);
    }
    std::vector<uint32_t> viewers(uint32_t now) {
        std::vector<uint32_t> result;
        for (auto it = renewed.begin(); it != renewed.end();) {
            if (!leaseActive(now, it->second)) {
                it = renewed.erase(it);
            } else {
                result.push_back(it->first);
                ++it;
            }
        }
        return result;
    }

  private:
    std::map<uint32_t, uint32_t> renewed;
};
} // namespace remote_console

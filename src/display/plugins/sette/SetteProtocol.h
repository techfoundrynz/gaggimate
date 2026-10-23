#ifndef SETTEPROTOCOL_H
#define SETTEPROTOCOL_H

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// Acaia-style framing spoken by a Sette 270Wi running the techfoundrynz grind-events firmware:
// EF DD <cmd> <payload...> <even-byte sum> <odd-byte sum>. Ported from sette-tool/src/lib/acaia.ts.
namespace sette {

constexpr uint8_t HEADER1 = 0xEF;
constexpr uint8_t HEADER2 = 0xDD;
constexpr uint8_t CMD_HEARTBEAT = 0x00;
constexpr uint8_t CMD_IDENT = 0x0B;
constexpr uint8_t CMD_EVENT = 0x0C;
constexpr uint8_t CMD_DISPLAY = 0x0E; // grind commands out, display event lines back

// Bounds the grinder firmware accepts for a target, in 0.1 g steps
constexpr float MIN_TARGET_GRAMS = 5.0f;
constexpr float MAX_TARGET_GRAMS = 50.0f;

enum class GrindStatus { NONE, PREPARING, COMPLETED, CANCELLED, FAILED, ENDED };

struct Message {
    enum class Kind { WEIGHT, GRIND, OTHER } kind = Kind::OTHER;
    float grams = 0.0f;
    GrindStatus status = GrindStatus::NONE;
    std::string line; // raw text of a display event line, recognised or not
};

inline void checksum(const uint8_t *payload, size_t len, uint8_t &even, uint8_t &odd) {
    unsigned a = 0, b = 0;
    for (size_t i = 0; i < len; i++) {
        if (i % 2 == 0) {
            a += payload[i];
        } else {
            b += payload[i];
        }
    }
    even = a & 0xFF;
    odd = b & 0xFF;
}

inline std::vector<uint8_t> encode(uint8_t cmd, const std::vector<uint8_t> &payload) {
    std::vector<uint8_t> out{HEADER1, HEADER2, cmd};
    out.insert(out.end(), payload.begin(), payload.end());
    uint8_t even, odd;
    checksum(payload.data(), payload.size(), even, odd);
    out.push_back(even);
    out.push_back(odd);
    return out;
}

inline std::vector<uint8_t> encodeHeartbeat() { return encode(CMD_HEARTBEAT, {2, 0}); }
inline std::vector<uint8_t> encodeIdent() {
    return encode(CMD_IDENT, {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x31, 0x32, 0x33, 0x34});
}
// Subscribes to weight (type 5) and the other stock event streams
inline std::vector<uint8_t> encodeNotificationRequest() { return encode(CMD_EVENT, {9, 0, 1, 1, 2, 2, 5, 3, 4}); }

inline bool isValidTarget(float grams) { return std::isfinite(grams) && grams >= MIN_TARGET_GRAMS && grams <= MAX_TARGET_GRAMS; }

// "GT,0180\r\n\0": sets the grinder's working dose (RAM only, not a preset). Ignored unless the grinder is idle.
inline std::vector<uint8_t> encodeSetTarget(float grams) {
    char body[16];
    const int tenths = static_cast<int>(std::lround(grams * 10.0f));
    const int len = snprintf(body, sizeof(body), "GT,%04d\r\n", tenths);
    std::vector<uint8_t> payload;
    payload.push_back(static_cast<uint8_t>(len + 2)); // length byte counts itself and the NUL
    payload.insert(payload.end(), body, body + len);
    payload.push_back(0);
    return encode(CMD_DISPLAY, payload);
}

inline bool decodeWeight(const uint8_t *p, size_t len, float &grams) {
    static constexpr float DIVISORS[] = {0.0f, 10.0f, 100.0f, 1000.0f, 10000.0f};
    if (len < 6 || p[4] == 0 || p[4] > 4) {
        return false;
    }
    grams = static_cast<float>((p[1] << 8) | p[0]) / DIVISORS[p[4]];
    if (p[5] & 0x02) {
        grams = -grams;
    }
    return true;
}

inline Message decodeDisplayEvent(const uint8_t *payload, size_t len) {
    Message m;
    if (len < 2 || len > 41) {
        return m;
    }
    std::string line(reinterpret_cast<const char *>(payload + 1), len - 1);
    while (!line.empty() && (line.back() == '\0' || line.back() == '\r' || line.back() == '\n')) {
        line.pop_back();
    }
    m.line = line;
    static constexpr struct {
        const char *line;
        GrindStatus status;
    } EVENTS[] = {{"E,G,P", GrindStatus::PREPARING}, {"E,G,C", GrindStatus::COMPLETED}, {"E,G,X", GrindStatus::CANCELLED},
                  {"E,G,F", GrindStatus::FAILED},    {"E,G,U", GrindStatus::ENDED}};
    for (const auto &e : EVENTS) {
        if (line == e.line) {
            m.kind = Message::Kind::GRIND;
            m.status = e.status;
        }
    }
    return m;
}

inline Message decodeEvent(uint8_t type, const uint8_t *p, size_t len) {
    Message m;
    float grams;
    if (type == 5 && decodeWeight(p, len, grams)) {
        m.kind = Message::Kind::WEIGHT;
        m.grams = grams;
    } else if (type == 11 && len > 3 && p[2] == 5 && decodeWeight(p + 3, len - 3, grams)) {
        m.kind = Message::Kind::WEIGHT;
        m.grams = grams;
    }
    return m;
}

// Streaming decoder: notifications may split or coalesce frames
class Decoder {
  public:
    template <typename F> void push(const uint8_t *data, size_t len, F &&onMessage) {
        buf.insert(buf.end(), data, data + len);
        if (buf.size() > MAX_BUFFER) {
            buf.erase(buf.begin(), buf.end() - MAX_BUFFER);
        }
        for (;;) {
            size_t s = 0;
            while (s + 1 < buf.size() && !(buf[s] == HEADER1 && buf[s + 1] == HEADER2)) {
                s++;
            }
            if (s + 1 >= buf.size()) {
                // Keep a trailing EF in case its DD arrives next
                buf.erase(buf.begin(), buf.end() - (buf.empty() ? 0 : 1));
                return;
            }
            buf.erase(buf.begin(), buf.begin() + s);
            if (buf.size() < 6) {
                return;
            }
            if (buf[3] < 1) {
                buf.erase(buf.begin());
                continue;
            }
            const size_t end = buf[3] + 5;
            if (buf.size() < end) {
                return;
            }
            const uint8_t *payload = buf.data() + 3;
            const size_t payloadLen = end - 5;
            uint8_t even, odd;
            checksum(payload, payloadLen, even, odd);
            if (buf[end - 2] != even || buf[end - 1] != odd) {
                checksumErrors++;
                buf.erase(buf.begin());
                continue;
            }
            frames++;
            const uint8_t cmd = buf[2];
            Message m;
            if (cmd == CMD_DISPLAY) {
                m = decodeDisplayEvent(payload, payloadLen);
            } else if (cmd == CMD_EVENT) {
                m = decodeEvent(buf[4], buf.data() + 5, end - 7);
            }
            buf.erase(buf.begin(), buf.begin() + end);
            if (m.kind != Message::Kind::OTHER || !m.line.empty()) {
                onMessage(m);
            }
        }
    }

    // Diagnostics, written only by the thread that calls push()
    uint32_t frames = 0;
    uint32_t checksumErrors = 0;

  private:
    static constexpr size_t MAX_BUFFER = 256;
    std::vector<uint8_t> buf;
};

} // namespace sette

#endif // SETTEPROTOCOL_H

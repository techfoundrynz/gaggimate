#ifndef SETTEPLUGIN_H
#define SETTEPLUGIN_H

#include "SetteProtocol.h"

#include <Arduino.h>
#include <atomic>
#include <cstdarg>
#include <deque>
#include <display/core/Plugin.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mutex>
#include <string>
#include <vector>

class NimBLEClient;
class NimBLERemoteCharacteristic;
class WebSocketHandler;

// Links a Sette 270Wi running the techfoundrynz grind-events firmware: GaggiMate's grind target becomes the
// grinder's target, and the weight of each completed grind becomes the dose of the next shot.
class SettePlugin : public Plugin {
  public:
    void setup(Controller *controller, PluginManager *pluginManager) override;
    void loop() override {}

  private:
    struct FoundGrinder {
        std::string name;
        std::string address;
        int rssi;
    };

    static void taskEntry(void *arg);
    void run();
    bool connect();
    void disconnect();
    bool write(const std::vector<uint8_t> &frame);
    std::string scanForGrinders(); // returns the nearest grinder's address, or empty
    void onNotify(const uint8_t *data, size_t len);
    void onMessage(const sette::Message &message);
    void onBrewStart();
    void setTarget(float grams);
    void registerWsHandlers(WebSocketHandler *ws);
    void handleDebugAction(const std::string &action);
    void log(const char *format, ...);
    void logLocked(const char *format, ...);
    void vlogLocked(const char *format, va_list args);
    static const char *statusName(sette::GrindStatus status);

    Controller *controller = nullptr;
    PluginManager *pluginManager = nullptr;
    TaskHandle_t taskHandle = nullptr;

    // Owned by the plugin task
    NimBLEClient *client = nullptr;
    NimBLERemoteCharacteristic *characteristic = nullptr;
    std::string address;
    unsigned long lastHeartbeatAt = 0;
    sette::Decoder decoder; // fed only from the NimBLE host task

    std::atomic<bool> controllerLinked{false};
    std::atomic<bool> scanRequested{false};
    std::atomic<bool> addressRequested{false};
    std::atomic<bool> reconnectRequested{false};
    std::atomic<unsigned long> nextConnectAt{0};
    std::atomic<uint32_t> rxFrames{0};
    std::atomic<uint32_t> rxChecksumErrors{0};

    // Shared between the plugin task, the NimBLE host task, the main loop and AsyncTCP
    std::mutex mutex;
    bool connected = false;
    float lastWeight = 0.0f;
    bool cycleActive = false;
    sette::GrindStatus lastStatus = sette::GrindStatus::NONE;
    float lastGrindWeight = 0.0f;
    float pendingDose = 0.0f; // weight of the last completed grind, applied to the next shot
    unsigned long pendingDoseAt = 0;
    float desiredTarget = 0.0f; // GaggiMate's grind target, pushed to the grinder
    bool targetDirty = false;
    float sentTarget = 0.0f;
    bool scanning = false;
    std::string requestedAddress;
    std::vector<FoundGrinder> found;

    // Diagnostics for the web UI's debug panel
    struct LogEntry {
        unsigned long at;
        std::string text;
    };
    std::deque<LogEntry> logEntries;
    std::string characteristicUuid;
    int rssi = 0;
    unsigned long connectedAt = 0;
    unsigned long lastRxAt = 0;
    uint32_t connectAttempts = 0;
    uint32_t connectFailures = 0;
    uint32_t txFrames = 0;
    uint32_t txFailures = 0;
};

#endif // SETTEPLUGIN_H

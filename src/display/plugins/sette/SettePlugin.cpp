#include "SettePlugin.h"

#include <NimBLEDevice.h>
#include <algorithm>
#include <cmath>
#include <display/core/Controller.h>
#include <display/core/process/BrewProcess.h>
#include <display/plugins/BLEScalePlugin.h>
#include <display/plugins/WebSocketHandler.h>
#include <display/util/PsramAllocator.h>

namespace {
constexpr const char *TAG = "SettePlugin";
constexpr const char *NAME_PREFIX = "BARW270";
constexpr uint8_t CONNECT_TIMEOUT_SECONDS = 5;
// The grinder's radio sleeps until a button press; each attempt briefly stops other BLE scans
constexpr unsigned long RETRY_MS = 20000;
constexpr unsigned long HEARTBEAT_MS = 2500;
constexpr unsigned long TASK_INTERVAL_MS = 50;
constexpr uint32_t SCAN_SECONDS = 5;
constexpr unsigned long OTA_PAUSE_TIMEOUT_MS = 15000;
// A grind older than this belongs to a shot that never happened
constexpr unsigned long DOSE_VALID_MS = 15 * 60 * 1000;
// Weights outside this range are not a dose (cup removed, scale glitch)
constexpr float MIN_DOSE_GRAMS = 3.0f;
constexpr float MAX_DOSE_GRAMS = 60.0f;
constexpr size_t LOG_ENTRIES = 40;

const NimBLEUUID SERVICE_UUID(static_cast<uint16_t>(0x1820));
const NimBLEUUID CHARACTERISTIC_UUID(static_cast<uint16_t>(0x2A80));
} // namespace

void SettePlugin::setup(Controller *_controller, PluginManager *_pluginManager) {
    controller = _controller;
    pluginManager = _pluginManager;

    pluginManager->on("controller:bluetooth:connect", [this](Event const &) { controllerLinked = true; });
    pluginManager->on("controller:bluetooth:disconnect", [this](Event const &) { controllerLinked = false; });
    pluginManager->on("controller:brew:start", [this](Event const &) { onBrewStart(); });
    // GaggiMate's grind target is the grinder's target; "grind target follows profile dose" drives it from profiles
    pluginManager->on("controller:grindVolume:change", [this](Event const &event) { setTarget(event.getFloat("value")); });
    pluginManager->on("webui:ws:register",
                      [this](Event const &event) { registerWsHandlers(event.getPointer<WebSocketHandler>("ws")); });

    pluginManager->on("ota:update:start", [this](Event &event) {
        otaPaused = false; // a retry needs a fresh acknowledgement, even if the worker has not resumed yet
        updating = true;
        const unsigned long startedAt = millis();
        // The worker owns the BLE client. Wait for its current scan/connect/write and the disconnect to finish;
        // setting a flag alone would let OTA race a scan or connect already in progress.
        while (taskHandle != nullptr && !otaPaused) {
            if (millis() - startedAt >= OTA_PAUSE_TIMEOUT_MS) {
                log("Could not pause grinder; firmware update cancelled");
                event.setInt("blocked", 1);
                return;
            }
            vTaskDelay(pdMS_TO_TICKS(TASK_INTERVAL_MS));
        }
    });
    pluginManager->on("ota:update:end", [this](Event const &) { updating = false; });

    setTarget(static_cast<float>(controller->getSettings().getTargetGrindVolume()));
    xTaskCreatePinnedToCore(taskEntry, "SettePlugin", 6144, this, 1, &taskHandle, 0);
}

void SettePlugin::taskEntry(void *arg) { static_cast<SettePlugin *>(arg)->run(); }

void SettePlugin::run() {
    for (;;) {
        if (updating) {
            if (!otaPaused) {
                disconnect();
                // NimBLE disconnect is asynchronous. Keep the grinder off the radio before acknowledging the pause.
                while (updating && client != nullptr && client->isConnected()) {
                    vTaskDelay(pdMS_TO_TICKS(TASK_INTERVAL_MS));
                }
                if (!updating) {
                    continue; // preparation timed out; OTA was cancelled
                }
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    connected = false;
                    cycleActive = false;
                    scanning = false;
                    logLocked("Grinder paused for firmware update");
                }
                otaPaused = true;
            }
            vTaskDelay(pdMS_TO_TICKS(TASK_INTERVAL_MS));
            continue;
        }
        if (otaPaused.exchange(false)) {
            log("Firmware update ended; resuming grinder link");
            nextConnectAt = millis();
            BLEScales.resumeScan(); // an in-flight grinder scan/connect may have stopped it before the pause
        }
        if (scanRequested.exchange(false)) {
            scanForGrinders();
        }
        if (addressRequested.exchange(false)) {
            std::string requested;
            {
                std::lock_guard<std::mutex> lock(mutex);
                requested = requestedAddress;
            }
            disconnect();
            controller->getSettings().setSetteAddress(String(requested.c_str())); // empty: find the nearest again
            if (requested.empty()) {
                log("Grinder forgotten");
            } else {
                log("Grinder set to %s", requested.c_str());
            }
            nextConnectAt = millis();
        }
        if (reconnectRequested.exchange(false)) {
            log("Reconnect requested");
            disconnect();
            nextConnectAt = millis();
        }
        const unsigned long now = millis();
        const bool linked = client != nullptr && client->isConnected() && characteristic != nullptr;
        bool wasConnected;
        {
            std::lock_guard<std::mutex> lock(mutex);
            wasConnected = connected;
            if (!linked && connected) {
                connected = false;
                cycleActive = false;
            }
        }
        if (!linked) {
            if (wasConnected) {
                log("Grinder disconnected");
                characteristic = nullptr;
                nextConnectAt = now + RETRY_MS;
            }
            address = controller->getSettings().getSetteAddress().c_str();
            // Never scan or connect while the controller link is down: its transport is scanning for the controller
            if (controllerLinked && static_cast<long>(now - nextConnectAt) >= 0) {
                if (address.empty()) {
                    // No grinder chosen yet: adopt the nearest one in range
                    nextConnectAt = millis() + RETRY_MS;
                    const std::string nearest = scanForGrinders();
                    if (!nearest.empty()) {
                        log("Adopted nearest grinder %s", nearest.c_str());
                        controller->getSettings().setSetteAddress(String(nearest.c_str()));
                        nextConnectAt = millis();
                    }
                } else if (!connect()) {
                    disconnect();
                    nextConnectAt = millis() + RETRY_MS;
                    std::lock_guard<std::mutex> lock(mutex);
                    connectFailures++;
                }
            }
        } else {
            if (now - lastHeartbeatAt >= HEARTBEAT_MS) {
                lastHeartbeatAt = now;
                write(sette::encodeHeartbeat());
                const int linkRssi = client->getRssi();
                std::lock_guard<std::mutex> lock(mutex);
                rssi = linkRssi;
            }
            float target = 0.0f;
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (targetDirty) {
                    targetDirty = false;
                    target = desiredTarget;
                }
            }
            // The grinder ignores the target unless it is idle on its dose screen; there is no acknowledgement
            if (sette::isValidTarget(target) && write(sette::encodeSetTarget(target))) {
                std::lock_guard<std::mutex> lock(mutex);
                sentTarget = target;
                logLocked("Sent target %.1f g", target);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(TASK_INTERVAL_MS));
    }
}

bool SettePlugin::connect() {
    if (updating) {
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(mutex);
        connectAttempts++;
    }
    if (client == nullptr) {
        if (NimBLEDevice::getClientListSize() >= CONFIG_BT_NIMBLE_MAX_CONNECTIONS) {
            log("No free BLE connection for the grinder (%d in use)", static_cast<int>(NimBLEDevice::getClientListSize()));
            return false;
        }
        client = NimBLEDevice::createClient();
        client->setConnectTimeout(CONNECT_TIMEOUT_SECONDS);
    }
    const bool scanWasRunning = NimBLEDevice::getScan()->isScanning();
    const bool ok = client->connect(NimBLEAddress(address));
    if (scanWasRunning && !updating) {
        BLEScales.resumeScan();
    }
    if (updating) {
        return false;
    }
    if (!ok) {
        log("Grinder %s not reachable", address.c_str());
        return false;
    }

    characteristic = nullptr;
    if (NimBLERemoteService *service = client->getService(SERVICE_UUID)) {
        characteristic = service->getCharacteristic(CHARACTERISTIC_UUID);
    }
    if (characteristic == nullptr) {
        // Fall back to the one characteristic that both notifies and takes writes, as sette-tool does
        for (NimBLERemoteService *service : *client->getServices(true)) {
            for (NimBLERemoteCharacteristic *c : *service->getCharacteristics(true)) {
                if (c->canNotify() && (c->canWriteNoResponse() || c->canWrite())) {
                    characteristic = c;
                    break;
                }
            }
            if (characteristic != nullptr)
                break;
        }
    }
    if (characteristic == nullptr) {
        log("Grinder %s has no usable characteristic", address.c_str());
        return false;
    }
    if (!characteristic->subscribe(
            true, [this](NimBLERemoteCharacteristic *, uint8_t *data, size_t len, bool) { onNotify(data, len); })) {
        log("Could not subscribe to grinder notifications");
        characteristic = nullptr;
        return false;
    }
    if (!write(sette::encodeIdent()) || !write(sette::encodeNotificationRequest())) {
        log("Could not send ident to grinder");
        characteristic = nullptr;
        return false;
    }
    lastHeartbeatAt = millis();
    {
        std::lock_guard<std::mutex> lock(mutex);
        connected = true;
        cycleActive = false;
        targetDirty = sette::isValidTarget(desiredTarget); // the grinder forgets the target on power cycle
        characteristicUuid = characteristic->getUUID().toString();
        connectedAt = millis();
        logLocked("Connected to grinder %s via %s", address.c_str(), characteristicUuid.c_str());
    }
    return true;
}

void SettePlugin::disconnect() {
    characteristic = nullptr;
    if (client != nullptr && client->isConnected()) {
        client->disconnect();
    }
}

bool SettePlugin::write(const std::vector<uint8_t> &frame) {
    if (updating || characteristic == nullptr) {
        return false;
    }
    const bool ok = characteristic->writeValue(frame.data(), frame.size(), !characteristic->canWriteNoResponse());
    std::lock_guard<std::mutex> lock(mutex);
    if (ok) {
        txFrames++;
    } else {
        txFailures++;
    }
    return ok;
}

std::string SettePlugin::scanForGrinders() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        found.clear();
        scanning = true;
    }
    std::vector<FoundGrinder> results;
    if (controllerLinked && !updating) {
        NimBLEScan *scan = NimBLEDevice::getScan();
        const bool scanWasRunning = scan->isScanning();
        scan->stop();
        scan->setAdvertisedDeviceCallbacks(nullptr);
        scan->setMaxResults(0xFF);
        scan->setActiveScan(true);
        NimBLEScanResults scanResults = scan->start(SCAN_SECONDS, false);
        for (int i = 0; i < scanResults.getCount(); i++) {
            NimBLEAdvertisedDevice device = scanResults.getDevice(i);
            if (device.getName().rfind(NAME_PREFIX, 0) == 0) {
                results.push_back({device.getName(), device.getAddress().toString(), device.getRSSI()});
            }
        }
        std::sort(results.begin(), results.end(), [](const FoundGrinder &a, const FoundGrinder &b) { return a.rssi > b.rssi; });
        scan->clearResults();
        scan->setMaxResults(0);
        if (scanWasRunning && !updating) {
            BLEScales.resumeScan();
        }
        log("Scan found %d grinder(s)", static_cast<int>(results.size()));
    } else {
        log("Not scanning for grinders while the controller link is down");
    }
    const std::string nearest = results.empty() ? std::string() : results.front().address;
    std::lock_guard<std::mutex> lock(mutex);
    found = std::move(results);
    scanning = false;
    return nearest;
}

void SettePlugin::onNotify(const uint8_t *data, size_t len) {
    if (updating) {
        return;
    }
    decoder.push(data, len, [this](const sette::Message &message) { onMessage(message); });
    rxFrames = decoder.frames;
    rxChecksumErrors = decoder.checksumErrors;
}

void SettePlugin::onMessage(const sette::Message &message) {
    std::lock_guard<std::mutex> lock(mutex);
    if (message.kind == sette::Message::Kind::WEIGHT) {
        lastWeight = message.grams;
        lastRxAt = millis();
        return;
    }
    lastRxAt = millis();
    logLocked("Grinder: %s", message.line.c_str());
    if (message.kind != sette::Message::Kind::GRIND)
        return;
    lastStatus = message.status;
    switch (message.status) {
    case sette::GrindStatus::PREPARING:
        cycleActive = true;
        lastWeight = 0.0f;
        break;
    case sette::GrindStatus::COMPLETED:
        // Sent after the grinder's own final-weight handling, while the cup is still on it
        if (cycleActive && lastWeight >= MIN_DOSE_GRAMS && lastWeight <= MAX_DOSE_GRAMS) {
            lastGrindWeight = lastWeight;
            pendingDose = lastWeight;
            pendingDoseAt = millis();
            logLocked("Grind completed: %.1f g, used as the next dose", lastWeight);
        } else if (!cycleActive) {
            logLocked("Grind completed without a start event; dose ignored");
        } else {
            logLocked("Grind completed at %.1f g, outside the dose range; ignored", lastWeight);
        }
        cycleActive = false;
        break;
    default:
        cycleActive = false;
        break;
    }
}

void SettePlugin::onBrewStart() {
    float dose = 0.0f;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (pendingDose > 0.0f && millis() - pendingDoseAt < DOSE_VALID_MS) {
            dose = pendingDose;
        }
        pendingDose = 0.0f;
    }
    if (dose <= 0.0f)
        return;
    // Only the shot's copy of the profile changes, so the saved and selected profile keep their dose
    float targetVolume = 0.0f;
    {
        std::lock_guard<std::recursive_mutex> guard(controller->getProcessLock());
        Process *process = controller->getProcess();
        if (process == nullptr || process->getType() != MODE_BREW)
            return;
        auto *brew = static_cast<BrewProcess *>(process);
        if (brew->isUtility())
            return;
        log("Shot dose %.1f g -> %.1f g from grinder", brew->profile.dose, dose);
        brew->profile.dose = dose;
        if (brew->profile.hasRatio())
            targetVolume = static_cast<float>(brew->getBrewVolume());
    }
    if (targetVolume > 0.0f) {
        pluginManager->trigger("controller:targetVolume:change", "value", targetVolume);
    }
}

void SettePlugin::setTarget(float grams) {
    std::lock_guard<std::mutex> lock(mutex);
    desiredTarget = grams;
    targetDirty = sette::isValidTarget(grams);
}

void SettePlugin::registerWsHandlers(WebSocketHandler *ws) {
    if (ws == nullptr)
        return;
    ws->registerRequestHandler("req:sette:status", [this, ws](uint32_t clientId, JsonDocument &request) {
        JsonDocument resp(&psramAllocator);
        resp["tp"] = "res:sette:status";
        if (request["rid"].is<const char *>())
            resp["rid"] = request["rid"];
        {
            std::lock_guard<std::mutex> lock(mutex);
            resp["connected"] = connected;
            resp["address"] = controller->getSettings().getSetteAddress();
            resp["target"] = sentTarget;
            resp["lastStatus"] = statusName(lastStatus);
            resp["lastGrindWeight"] = lastGrindWeight;
            resp["pendingDose"] = pendingDose;
            resp["scanning"] = scanning;
            JsonArray list = resp["found"].to<JsonArray>();
            for (const auto &grinder : found) {
                JsonObject entry = list.add<JsonObject>();
                entry["name"] = grinder.name;
                entry["address"] = grinder.address;
                entry["rssi"] = grinder.rssi;
            }
        }
        ws->sendJson(clientId, resp);
    });
    // Chooses the grinder; an empty address forgets it so the nearest one is adopted again
    ws->registerRequestHandler("req:sette:set-address", [this, ws](uint32_t clientId, JsonDocument &request) {
        std::string requested = request["address"] | "";
        std::transform(requested.begin(), requested.end(), requested.begin(), ::tolower);
        const bool valid = requested.empty() || (requested.size() == 17 && NimBLEAddress(requested) != NimBLEAddress(""));
        if (valid) {
            {
                std::lock_guard<std::mutex> lock(mutex);
                requestedAddress = requested;
            }
            addressRequested = true;
        }
        JsonDocument resp(&psramAllocator);
        resp["tp"] = "res:sette:set-address";
        resp["ok"] = valid;
        if (request["rid"].is<const char *>())
            resp["rid"] = request["rid"];
        ws->sendJson(clientId, resp);
    });
    ws->registerRequestHandler("req:sette:debug", [this, ws](uint32_t clientId, JsonDocument &request) {
        JsonDocument resp(&psramAllocator);
        resp["tp"] = "res:sette:debug";
        if (request["rid"].is<const char *>())
            resp["rid"] = request["rid"];
        const unsigned long now = millis();
        resp["uptime"] = now;
        resp["controllerLinked"] = controllerLinked.load();
        resp["otaPaused"] = otaPaused.load();
        resp["bleClients"] = NimBLEDevice::getClientListSize();
        resp["bleMaxClients"] = CONFIG_BT_NIMBLE_MAX_CONNECTIONS;
        resp["nextConnectIn"] = std::max(0L, static_cast<long>(nextConnectAt.load() - now));
        resp["rxFrames"] = rxFrames.load();
        resp["rxChecksumErrors"] = rxChecksumErrors.load();
        {
            std::lock_guard<std::mutex> lock(mutex);
            resp["connected"] = connected;
            resp["characteristic"] = characteristicUuid;
            resp["rssi"] = connected ? rssi : 0;
            resp["connectedFor"] = connected ? now - connectedAt : 0;
            resp["lastRxAgo"] = lastRxAt > 0 ? now - lastRxAt : 0;
            resp["liveWeight"] = lastWeight;
            resp["cycleActive"] = cycleActive;
            resp["desiredTarget"] = desiredTarget;
            resp["sentTarget"] = sentTarget;
            resp["pendingDose"] = pendingDose;
            resp["pendingDoseAge"] = pendingDose > 0.0f ? now - pendingDoseAt : 0;
            resp["connectAttempts"] = connectAttempts;
            resp["connectFailures"] = connectFailures;
            resp["txFrames"] = txFrames;
            resp["txFailures"] = txFailures;
            JsonArray entries = resp["log"].to<JsonArray>();
            for (const auto &entry : logEntries) {
                JsonObject e = entries.add<JsonObject>();
                e["ago"] = now - entry.at;
                e["text"] = entry.text;
            }
        }
        ws->sendJson(clientId, resp);
    });
    ws->registerRequestHandler("req:sette:debug-action", [this, ws](uint32_t clientId, JsonDocument &request) {
        handleDebugAction(request["action"] | "");
        JsonDocument resp(&psramAllocator);
        resp["tp"] = "res:sette:debug-action";
        if (request["rid"].is<const char *>())
            resp["rid"] = request["rid"];
        ws->sendJson(clientId, resp);
    });
    ws->registerRequestHandler("req:sette:scan", [this, ws](uint32_t clientId, JsonDocument &request) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            scanning = true; // reported right away so a status poll cannot see the old result
        }
        scanRequested = true;
        JsonDocument resp(&psramAllocator);
        resp["tp"] = "res:sette:scan";
        if (request["rid"].is<const char *>())
            resp["rid"] = request["rid"];
        ws->sendJson(clientId, resp);
    });
}

void SettePlugin::handleDebugAction(const std::string &action) {
    if (action == "reconnect") {
        reconnectRequested = true;
        return;
    }
    std::lock_guard<std::mutex> lock(mutex);
    if (action == "resend-target") {
        targetDirty = sette::isValidTarget(desiredTarget);
        if (targetDirty) {
            logLocked("Resending target %.1f g", desiredTarget);
        } else {
            logLocked("No valid target to resend (%.1f g)", desiredTarget);
        }
    } else if (action == "simulate-grind") {
        // Exercises the dose hand-off without grinding: the next shot takes GaggiMate's grind target as its dose
        if (desiredTarget >= MIN_DOSE_GRAMS && desiredTarget <= MAX_DOSE_GRAMS) {
            lastStatus = sette::GrindStatus::COMPLETED;
            lastGrindWeight = desiredTarget;
            pendingDose = desiredTarget;
            pendingDoseAt = millis();
            logLocked("Simulated grind: %.1f g, used as the next dose", desiredTarget);
        } else {
            logLocked("Cannot simulate a grind: target %.1f g is not a dose", desiredTarget);
        }
    } else if (action == "clear-dose") {
        pendingDose = 0.0f;
        logLocked("Pending dose cleared");
    } else if (action == "clear-log") {
        logEntries.clear();
        connectAttempts = connectFailures = txFrames = txFailures = 0;
    }
}

void SettePlugin::log(const char *format, ...) {
    va_list args;
    va_start(args, format);
    {
        std::lock_guard<std::mutex> lock(mutex);
        vlogLocked(format, args);
    }
    va_end(args);
}

void SettePlugin::logLocked(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vlogLocked(format, args);
    va_end(args);
}

void SettePlugin::vlogLocked(const char *format, va_list args) {
    char text[128];
    vsnprintf(text, sizeof(text), format, args);
    ESP_LOGI(TAG, "%s", text);
    logEntries.push_back({millis(), text});
    while (logEntries.size() > LOG_ENTRIES) {
        logEntries.pop_front();
    }
}

const char *SettePlugin::statusName(sette::GrindStatus status) {
    switch (status) {
    case sette::GrindStatus::PREPARING:
        return "preparing";
    case sette::GrindStatus::COMPLETED:
        return "completed";
    case sette::GrindStatus::CANCELLED:
        return "cancelled";
    case sette::GrindStatus::FAILED:
        return "failed";
    case sette::GrindStatus::ENDED:
        return "ended";
    default:
        return "none";
    }
}

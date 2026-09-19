#pragma once
#include <stdint.h>
#include <display/core/ButtonHandler.h>

enum class EncoderPage { Other, Standby, Brew, BrewStatus, Steam, Water, Grind };
enum class EncoderAction { None, BrewToggle, BrewDismiss, WaterToggle, GrindToggle, Profile, Temperature, Weight };

inline EncoderAction encoderClickAction(EncoderPage page) {
    switch (page) {
    case EncoderPage::Brew: return EncoderAction::BrewToggle;
    case EncoderPage::BrewStatus: return EncoderAction::BrewDismiss;
    case EncoderPage::Water: return EncoderAction::WaterToggle;
    case EncoderPage::Grind: return EncoderAction::GrindToggle;
    default: return EncoderAction::None;
    }
}
inline EncoderAction encoderDialAction(EncoderPage page) {
    switch (page) {
    case EncoderPage::Brew: return EncoderAction::Profile;
    case EncoderPage::Steam:
    case EncoderPage::Water: return EncoderAction::Temperature;
    case EncoderPage::Grind: return EncoderAction::Weight;
    default: return EncoderAction::None;
    }
}

// ButtonHandler owns click/long-hold recognition. This adapter only prevents
// a short click from transferring onto a different screen during a held press.
class EncoderPress {
  public:
    enum class Event { None, Click, LongPress };
    EncoderPress() {
        ButtonHandler::Config config;
        config.momentary = true;
        config.longPress[0] = true;
        config.longPressMs = 2000;
        handler.setConfig(config);
        handler.setCallback([this](uint8_t, ButtonHandler::Event action) {
            if (action == ButtonHandler::Event::LONG_PRESS) {
                event = Event::LongPress;
            } else if (action == ButtonHandler::Event::CLICK && !cancelled) {
                event = Event::Click;
            }
        });
    }
    int startScreen() const { return initialScreen; }
    bool stayedOnScreen() const { return !cancelled; }
    void reset() {
        handler.reset();
        held = cancelled = false;
        event = Event::None;
    }
    Event update(bool pressed, uint32_t now, int screen) {
        event = Event::None;
        if (held && screen != initialScreen) {
            cancelled = true;
        }
        // Advance at the sampled edge time before delivering release. A stalled
        // UI must neither miss a long hold nor stretch a queued short tap.
        handler.loop(now);
        if (pressed != held) {
            if (pressed) {
                initialScreen = screen;
                cancelled = false;
            }
            held = pressed;
            handler.onRawState(0, pressed, now);
        }
        return event;
    }
  private:
    ButtonHandler handler;
    Event event = Event::None;
    bool held = false, cancelled = false;
    int initialScreen = 0;
};

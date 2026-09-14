#include "DefaultUI.h"

#ifndef GAGGIMATE_SIM
#include "EncoderControls.h"
#include "PressFeedback.h"
#include "eez/actions.h"
#include <display/core/Controller.h>
#include <display/drivers/common/Encoder.h>
#include <algorithm>

void DefaultUI::updateEncoderControls() {
    static EncoderPress press;
    static bool encoderFlushHeld = false;
    const auto input = readEncoder();
    // A flush changes to the status screen. Always deliver release before any
    // screen/readiness guards, including when the input queue loses an edge.
    if (encoderFlushHeld && (!input.pressed || input.cancelled || !controller->isActive())) {
        controller->onFlushRelease();
        encoderFlushHeld = false;
    }
    if (input.cancelled) {
        press.reset();
        return;
    }
    const auto screen = static_cast<ScreensEnum>(eez_flow_get_current_screen());
    const auto event = press.update(input.pressed, input.at, screen);
    const bool brewHold = press.startScreen() == SCREEN_ID_BREW_SCREEN;
    if (event == EncoderPress::Event::LongPress && !brewHold) {
        controller->activateStandby();
        changeScreen(SCREEN_ID_STANDBY_SCREEN);
        return;
    }

    EncoderPage page = EncoderPage::Other;
    switch (screen) {
    case SCREEN_ID_STANDBY_SCREEN: page = EncoderPage::Standby; break;
    case SCREEN_ID_BREW_SCREEN: page = EncoderPage::Brew; break;
    case SCREEN_ID_STATUS_SCREEN: page = EncoderPage::BrewStatus; break;
    case SCREEN_ID_STEAM_SCREEN: page = EncoderPage::Steam; break;
    case SCREEN_ID_WATER_SCREEN: page = EncoderPage::Water; break;
    case SCREEN_ID_GRIND_SCREEN: page = EncoderPage::Grind; break;
    default: break;
    }
    // Do not act on a screen that is about to be replaced, or through a warning.
    // Wake uses exactly the same readiness/connection checks as touch.
    if (targetScreen != screen || brewConfirmVisible) return;
    if (event == EncoderPress::Event::Click && page == EncoderPage::Standby) {
        pulsePressFeedback(objects.touch_icon);
        action_on_wakeup(nullptr);
        return;
    }
    if (!controller->isReady() || !controller->getClientController()->isConnected() ||
        controller->getSystemInfo().protocolMismatch || controller->isErrorState()) return;

    if (event == EncoderPress::Event::LongPress && brewHold) {
        // Do not replay a queued hold after release or start a flush if the
        // gesture crossed screens. An active brew must never be replaced.
        if (input.pressed && press.stayedOnScreen() && page == EncoderPage::Brew &&
            controller->getMode() == MODE_BREW && !controller->isActive()) {
            controller->onFlush(true);
            encoderFlushHeld = true;
        }
        return;
    }

    if (event == EncoderPress::Event::Click) {
        switch (encoderClickAction(page)) {
        case EncoderAction::BrewDismiss:
            // Match both status-screen buttons: stop while running, or
            // acknowledge completion without accidentally starting another brew.
            if (controller->getMode() == MODE_BREW) {
                pulsePressFeedback(controller->isActive() ? objects.pause_button : objects.check_button);
                action_on_brew_cancel(nullptr);
            }
            break;
        case EncoderAction::BrewToggle:
            if (controller->getMode() != MODE_BREW) break;
            pulsePressFeedback(objects.start_button);
            if (controller->isActive()) action_on_brew_cancel(nullptr);
            else action_on_brew_start(nullptr);
            break;
        case EncoderAction::WaterToggle:
            if (controller->getMode() == MODE_WATER) {
                pulsePressFeedback(objects.water_start_button);
                action_on_simple_process_toggle(nullptr);
            }
            break;
        case EncoderAction::GrindToggle:
            if (controller->getMode() == MODE_GRIND) {
                pulsePressFeedback(objects.grind_start_button);
                action_on_grind_toggle(nullptr);
            }
            break;
        default: break;
        }
    }
    if (!input.steps) return;
    switch (encoderDialAction(page)) {
    case EncoderAction::Profile: {
        if (controller->isActive() || controller->getMode() != MODE_BREW || !profileLoaded) break;
        String id;
        {
            // Unlike the preview list, this order does not move the selected item to the front.
            auto ids = profileManager->getFavoritedProfiles();
            const auto selected = controller->getSettings().getSelectedProfile();
            if (std::find(ids.begin(), ids.end(), selected) == ids.end()) ids.insert(ids.begin(), selected);
            int count = static_cast<int>(ids.size());
            if (!count) break;
            int index = 0;
            for (int i = 0; i < count; ++i) if (ids[i] == selected) index = i;
            index = ((index + input.steps) % count + count) % count;
            id = ids[index];
        }
        profileManager->selectProfile(id);
        profileDirty = false;
        controller->updateLastAction();
        rerender = true;
        break;
    }
    case EncoderAction::Temperature:
        if ((page == EncoderPage::Steam && controller->getMode() != MODE_STEAM) ||
            (page == EncoderPage::Water && controller->getMode() != MODE_WATER)) break;
        for (int i = 0; i < abs(input.steps); ++i)
            input.steps > 0 ? controller->raiseTemp() : controller->lowerTemp();
        break;
    case EncoderAction::Weight:
        if (controller->getMode() != MODE_GRIND) break;
        controller->setTargetGrindVolume(constrain(controller->getSettings().getTargetGrindVolume() + input.steps * 0.5,
                                                  BREW_MIN_VOLUMETRIC, BREW_MAX_VOLUMETRIC));
        break;
    default: break;
    }
}
#endif

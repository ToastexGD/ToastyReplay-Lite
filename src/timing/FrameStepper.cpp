#include "FrameStepper.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/PlayLayer.hpp>

#include <chrono>

using namespace geode::prelude;

namespace {
    using Clock = std::chrono::steady_clock;
    constexpr auto RepeatDelay = std::chrono::milliseconds(300);

    bool s_enabled = false;
    bool s_repeating = false;
    int s_pending = 0;
    Clock::time_point s_repeatSince;
} // namespace

namespace toasty::stepper {
    bool enabled() {
        return s_enabled;
    }

    void setEnabled(bool value) {
        s_enabled = value;
        s_pending = 0;
        s_repeating = false;
        if (Mod::get()->getSavedValue<bool>("frame-stepper", false) != value) {
            Mod::get()->setSavedValue<bool>("frame-stepper", value);
        }
    }

    bool overridesTps() {
        return Mod::get()->getSavedValue<bool>("stepper-override-tps", false);
    }

    bool freezes() {
        return s_enabled && PlayLayer::get();
    }

    void stepOnce() {
        if (s_enabled) {
            s_pending++;
        }
    }

    void setRepeating(bool repeating) {
        if (!s_enabled) {
            s_repeating = false;
            return;
        }
        if (repeating && !s_repeating) {
            s_repeatSince = Clock::now();
        }
        s_repeating = repeating;
    }

    bool takeStep() {
        if (s_pending > 0) {
            s_pending--;
            return true;
        }
        return s_repeating && Clock::now() - s_repeatSince >= RepeatDelay;
    }
} // namespace toasty::stepper

$on_mod(Loaded) {
    s_enabled = Mod::get()->getSavedValue<bool>("frame-stepper", false);
}

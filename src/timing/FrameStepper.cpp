#include "FrameStepper.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/PlayLayer.hpp>

using namespace geode::prelude;

namespace {
    bool s_enabled = false;
    int s_pending = 0;
} // namespace

namespace toasty::stepper {
    bool enabled() {
        return s_enabled;
    }

    void setEnabled(bool value) {
        s_enabled = value;
        s_pending = 0;
        if (Mod::get()->getSavedValue<bool>("frame-stepper", false) != value) {
            Mod::get()->setSavedValue<bool>("frame-stepper", value);
        }
    }

    bool freezes() {
        return s_enabled && PlayLayer::get();
    }

    void step() {
        if (s_enabled) {
            s_pending++;
        }
    }

    bool takeStep() {
        if (s_pending <= 0) {
            return false;
        }
        s_pending--;
        return true;
    }
} // namespace toasty::stepper

$on_mod(Loaded) {
    s_enabled = Mod::get()->getSavedValue<bool>("frame-stepper", false);
}

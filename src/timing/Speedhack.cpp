#include "Speedhack.hpp"

#include <Geode/Geode.hpp>
#include <Geode/loader/GameEvent.hpp>

#include <algorithm>
#include <cmath>

using namespace geode::prelude;

namespace {
    bool s_enabled = false;
    double s_rate = toasty::speedhack::Default;
    bool s_gameLoaded = false;

    double boundedRate(double value) {
        if (!std::isfinite(value)) {
            return toasty::speedhack::Default;
        }
        return std::clamp(value, toasty::speedhack::Minimum, toasty::speedhack::Maximum);
    }

    void applySpeed() {
        if (!s_gameLoaded) {
            return;
        }
        if (auto director = CCDirector::sharedDirector()) {
            if (auto scheduler = director->getScheduler()) {
                scheduler->setTimeScale(
                    static_cast<float>(s_enabled ? s_rate : toasty::speedhack::Default));
            }
        }
    }
}

namespace toasty::speedhack {
    bool enabled() {
        return s_enabled;
    }

    double rate() {
        return s_rate;
    }

    void setEnabled(bool value) {
        s_enabled = value;
        if (Mod::get()->getSavedValue<bool>("speedhack", false) != value) {
            Mod::get()->setSavedValue<bool>("speedhack", value);
        }
        applySpeed();
    }

    void setRate(double value) {
        s_rate = boundedRate(value);
        if (Mod::get()->getSavedValue<double>("speedhack-rate", Default) != s_rate) {
            Mod::get()->setSavedValue<double>("speedhack-rate", s_rate);
        }
        applySpeed();
    }
}

$on_mod(Loaded) {
    s_rate = boundedRate(
        Mod::get()->getSavedValue<double>("speedhack-rate", toasty::speedhack::Default));
    s_enabled = Mod::get()->getSavedValue<bool>("speedhack", false);
}

$on_game(Loaded) {
    s_gameLoaded = true;
    applySpeed();
}

#include "TpsBypass.hpp"
#include "FrameStepper.hpp"
#include "StepPlanner.hpp"
#include "TpsPatch.hpp"

#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

using namespace geode::prelude;

namespace {
    bool s_enabled = false;
    int64_t s_rate = toasty::tps::Vanilla;
    std::optional<int64_t> s_replayRate;

    int64_t boundedRate(int64_t value) {
        return std::clamp<int64_t>(value, toasty::tps::Minimum, toasty::tps::Maximum);
    }

    bool customTimingActive() {
        return toasty::tps::patch::interceptsTicks();
    }

    int64_t activeRate() {
        if (s_replayRate) {
            return *s_replayRate;
        }
        return s_enabled ? s_rate : toasty::tps::Vanilla;
    }
} // namespace

namespace toasty::tps {
    bool available() {
        return patch::available();
    }

    bool enabled() {
        return s_enabled && patch::available();
    }

    bool setEnabled(bool value) {
        if (value && !patch::available())
            return false;
        if (!s_replayRate && !patch::setEnabled(value))
            return false;
        s_enabled = value;
        if (Mod::get()->getSavedValue<bool>("tps-bypass", false) != value) {
            Mod::get()->setSavedValue<bool>("tps-bypass", value);
        }
        return true;
    }

    int64_t rate() {
        return s_rate;
    }

    int64_t effectiveRate() {
        return activeRate();
    }

    void setRate(int64_t value) {
        auto bounded = boundedRate(value);
        s_rate = bounded;
        if (Mod::get()->getSavedValue<int64_t>("tps-rate", Vanilla) != bounded) {
            Mod::get()->setSavedValue<int64_t>("tps-rate", bounded);
        }
    }

    bool beginReplayOverride(int64_t value) {
        if (!patch::available() || value < Minimum || value > Maximum) {
            return false;
        }
        auto bounded = boundedRate(value);
        if (!patch::setEnabled(true)) {
            static_cast<void>(patch::setEnabled(s_enabled));
            return false;
        }
        s_replayRate = bounded;
        return true;
    }

    void endReplayOverride() {
        if (!s_replayRate) {
            return;
        }
        s_replayRate.reset();
        if (!patch::setEnabled(s_enabled)) {
            s_enabled = false;
            Mod::get()->setSavedValue<bool>("tps-bypass", false);
        }
    }

    std::string const& unavailableReason() {
        return patch::error();
    }
} // namespace toasty::tps

class $modify(ToastyTpsGameLayer, GJBaseGameLayer) {
    struct Fields {
        toasty::timing::StepPlanner planner;
        bool customDelta = false;
        double delta = 0.0;
    };

    double getModifiedDelta(float dt) {
        auto fields = m_fields.self();
        if (fields->customDelta)
            return fields->delta;
        return GJBaseGameLayer::getModifiedDelta(dt);
    }

    void update(float dt) override {
        auto fields = m_fields.self();

        if (toasty::stepper::freezes()) {
            if (!toasty::stepper::takeStep()) {
                return;
            }
            auto stepRate = toasty::stepper::overridesTps() ? toasty::tps::Vanilla : activeRate();
            fields->planner.reset();
            fields->customDelta = false;
            if (toasty::tps::patch::interceptsTicks())
                toasty::tps::patch::setExpected(1);
            GJBaseGameLayer::update(static_cast<float>(1.0 / stepRate));
            return;
        }

        auto active = customTimingActive();
        auto target = activeRate();
        auto timeWarp = std::min(static_cast<double>(m_gameState.m_timeWarp), 1.0);

        if (!active || !std::isfinite(dt) || dt < 0.f || !std::isfinite(timeWarp) ||
            timeWarp <= 0.0) {
            fields->planner.reset();
            fields->customDelta = false;
            if (toasty::tps::patch::interceptsTicks())
                toasty::tps::patch::setExpected(1);
            GJBaseGameLayer::update(dt);
            return;
        }

        if (m_resumeTimer > 0) {
            fields->planner.reset();
            fields->customDelta = false;
            toasty::tps::patch::setExpected(1);
            GJBaseGameLayer::update(dt);
            return;
        }

        auto timestep = timeWarp / static_cast<double>(target);
        auto plan = fields->planner.advance(static_cast<double>(dt), timestep);
        fields->delta = plan.delta;
        fields->customDelta = true;
        toasty::tps::patch::setExpected(plan.steps);
        GJBaseGameLayer::update(static_cast<float>(plan.delta));
        fields->customDelta = false;
    }
};

class $modify(ToastyTpsPlayLayer, PlayLayer) {
    bool needsProgressFix() const {
        return (s_enabled || s_replayRate.has_value()) && activeRate() != toasty::tps::Vanilla &&
               m_level && m_level->m_timestamp > 0;
    }

    unsigned int correctedProgress() const {
        auto ticks = std::round(std::max(0.0, m_gameState.m_levelTime) * 480.0);
        return static_cast<unsigned int>(
            std::min(ticks, static_cast<double>(std::numeric_limits<unsigned int>::max())));
    }

    void updateProgressbar() {
        auto original = m_gameState.m_currentProgress;
        if (needsProgressFix())
            m_gameState.m_currentProgress = correctedProgress();
        PlayLayer::updateProgressbar();
        m_gameState.m_currentProgress = original;
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) override {
        auto original = m_gameState.m_currentProgress;
        if (needsProgressFix())
            m_gameState.m_currentProgress = correctedProgress();
        PlayLayer::destroyPlayer(player, object);
        m_gameState.m_currentProgress = original;
    }

    void levelComplete() {
        auto original = m_gameState.m_commandIndex;
        if (needsProgressFix())
            m_gameState.m_commandIndex = correctedProgress();
        PlayLayer::levelComplete();
        m_gameState.m_commandIndex = original;
    }
};

$on_mod(Loaded) {
    toasty::tps::patch::initialize();
    s_rate = boundedRate(Mod::get()->getSavedValue<int64_t>("tps-rate", toasty::tps::Vanilla));
    s_enabled = Mod::get()->getSavedValue<bool>("tps-bypass", false);
    if (!toasty::tps::setEnabled(s_enabled)) {
        s_enabled = false;
        Mod::get()->setSavedValue<bool>("tps-bypass", false);
    }
}

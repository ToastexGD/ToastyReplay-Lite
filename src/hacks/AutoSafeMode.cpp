#include <Geode/modify/PlayLayer.hpp>
#include "../engine/Engine.hpp"
using namespace geode::prelude;

static std::unordered_set<std::string> s_cheats = {};

bool hasBeenReplaying;

namespace {
    bool isUnsafe() {
        if ((!s_cheats.empty() || hasBeenReplaying) && Mod::get()->getSavedValue<bool>("auto-safe-mode", true)) {
            return true;
        } else {
            return false;
        }
        
    }
} // namespace

class $modify(AutoSafeModePlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false; 
        }
        this->schedule(schedule_selector(AutoSafeModePlayLayer::checkValues));
        return true;
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        s_cheats.clear();
        hasBeenReplaying = false;
    }
    void levelComplete() {
        if (!isUnsafe()) {
            PlayLayer::levelComplete();
            return;
        }
        auto previous = m_isTestMode;
        m_isTestMode = true;
        PlayLayer::levelComplete();
        m_isTestMode = previous;
    }
    
    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (object == m_anticheatSpike) {
            return PlayLayer::destroyPlayer(player, object);
        }

        if (!isUnsafe()) {
            PlayLayer::destroyPlayer(player, object);
            return;
        } else {
            auto previous = m_isTestMode;
            m_isTestMode = true;
            PlayLayer::destroyPlayer(player, object);
            m_isTestMode = previous;
        }
    }
    
    void checkValues(float dt) {

        for (auto key : {"noclip","speedhack","tps-bypass","frame-stepper","set-seed"}) {
            if (Mod::get()->getSavedValue<bool>(key, false)) {
                s_cheats.insert(key);
            }
        }

        if (bool isPlaying = toasty::engine::playing()) {
            hasBeenReplaying = true;
        }
    }

    void showNewBest(bool newReward, int orbs, int diamonds, bool demonKey, bool noRetry, bool noTitle) {
        if (isUnsafe()) {
            return;
        }
        PlayLayer::showNewBest(newReward, orbs, diamonds, demonKey, noRetry, noTitle);
    }
};

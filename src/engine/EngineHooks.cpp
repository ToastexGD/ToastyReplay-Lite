#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/binding/PlayLayer.hpp>

#include "Engine.hpp"

using namespace geode::prelude;

using toasty::replay::InputButton;
using toasty::replay::InputPlayer;

class $modify(GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        if (toasty::engine::recording() && PlayLayer::get()) {
            if (button >= static_cast<int>(InputButton::Jump) &&
                button <= static_cast<int>(InputButton::Right)) {
                auto tick = static_cast<uint64_t>(m_currentStep);
                auto player = isPlayer1 ? InputPlayer::Player1 : InputPlayer::Player2;
                toasty::engine::capture(tick, static_cast<InputButton>(button), player, down);
                if (Mod::get()->getSavedValue<bool>("trace-inputs", false)) {
                    log::debug("record tick={} button={} player={} down={}",
                               tick,
                               button,
                               isPlayer1 ? 1 : 2,
                               down);
                }
            }
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }
};

class $modify(PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }
        toasty::engine::beginLevelSession(static_cast<uint64_t>(level->m_levelID.value()),
                                          static_cast<uint64_t>(level->m_levelVersion));
        toasty::engine::beginAttempt();
        return true;
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        toasty::engine::beginAttempt();
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        PlayLayer::destroyPlayer(player, object);
        toasty::engine::endAttempt();
    }

    void levelComplete() {
        PlayLayer::levelComplete();
        toasty::engine::endAttempt();
    }

    void onQuit() {
        PlayLayer::onQuit();
        toasty::engine::cancelAttempt();
    }
};

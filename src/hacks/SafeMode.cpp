#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

namespace {
    bool safeModeEnabled() {
        return Mod::get()->getSavedValue<bool>("safe-mode", false);
    }
} // namespace

class $modify(SafeModePlayLayer, PlayLayer) {
    void levelComplete() {
        if (!safeModeEnabled()) {
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
        if (!safeModeEnabled()) {
            PlayLayer::destroyPlayer(player, object);
            return;
        }

        auto previous = m_isTestMode;
        m_isTestMode = true;
        PlayLayer::destroyPlayer(player, object);
        m_isTestMode = previous;
        
    }

};

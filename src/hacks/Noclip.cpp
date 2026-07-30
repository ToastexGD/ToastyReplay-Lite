#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

class $modify(PlayLayer) {
    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (object == m_anticheatSpike) {
            return PlayLayer::destroyPlayer(player, object);
        }

        if (Mod::get()->getSavedValue<bool>("noclip", false)) {
            return;
        }

        PlayLayer::destroyPlayer(player, object);
    }
};
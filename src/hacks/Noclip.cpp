#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

class $modify(PlayLayer) {
    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (object == m_anticheatSpike) {
            return PlayLayer::destroyPlayer(player, object);
        }

        if (Mod::get()->getSavedValue<bool>("noclip", false)) {
            auto second = player && player == m_player2 && m_player2 != m_player1;
            auto key = second ? "noclip-p2" : "noclip-p1";
            if (Mod::get()->getSavedValue<bool>(key, true)) {
                return;
            }
        }

        PlayLayer::destroyPlayer(player, object);
    }
};

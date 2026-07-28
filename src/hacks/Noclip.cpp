#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

class $modify(PlayLayer) {
    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (Mod::get()->getSavedValue<bool>("noclip",false) && object != m_anticheatSpike) {
            return;
        }
        
        PlayLayer::destroyPlayer(player, object);
    }
};
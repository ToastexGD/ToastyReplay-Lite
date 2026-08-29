#include <Geode/Geode.hpp>
#include <fryy_55.amber/include/classes/DraggableButton.hpp>
#include <Geode/modify/MenuLayer.hpp>

#include "../ToastyMenu.hpp"
#include "buttonUtils.hpp"

using namespace geode::prelude;



$on_mod(Loaded) {
    listenForSettingChanges<bool>("show-floating-button", [](bool value) {
        if (value) {
            tryAddButton();
        } else {
            tryDeleteButton();
        }
    });
}

class $modify(FloatingButtonMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) {
            return false;
        }


        if (!Mod::get()->setSavedValue("checked-platform-for-floating-button", true)) {
            #if defined(GEODE_IS_MOBILE)
                    Mod::get()->setSettingValue("show-floating-button", true);
            #else
                    Mod::get()->setSettingValue("show-floating-button", false);
            #endif
        }

        if (!Mod::get()->getSettingValue<bool>("show-floating-button")) {
            return true;
        }

        tryAddButton();

        return true;
    }  
};



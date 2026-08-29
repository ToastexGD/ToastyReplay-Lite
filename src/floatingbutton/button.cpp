#include <Geode/Geode.hpp>
#include <fryy_55.amber/include/classes/DraggableButton.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/binding/GJBaseGameLayer.hpp>
#include <Geode/ui/SceneEvent.hpp>

#include "../ToastyMenu.hpp"
#include "../engine/Engine.hpp"
#include "button.hpp"
#include "buttonUtils.hpp"

using namespace geode::prelude;

namespace toasty::ui {
    namespace detail {
        CCScene* incomingScene(CCScene* scene) {
            if (auto transition = typeinfo_cast<CCTransitionScene*>(scene)) {
                return transition->m_pInScene;
            }
            return scene;
        }

        bool menuScene(CCScene* scene) {
            auto target = incomingScene(scene);
            if (!target) {
                return false;
            }
            return target->getChildByType<GJBaseGameLayer>(0) == nullptr;
        }

        void applyFloatingButton(CCScene* scene) {
            auto ovM = OverlayManager::get();
            if (!ovM) {
                return;
            }
            if (auto button = ovM->getChildByID("draggableMenuButton"_spr)) {
                button->setVisible(menuScene(scene));
            }
        }
    } // namespace detail

    void refreshFloatingButton() {
        detail::applyFloatingButton(CCDirector::get()->getRunningScene());
    }
} // namespace toasty::ui

$on_mod(Loaded) {
    listenForSettingChanges<bool>("show-floating-button", [](bool value) {
        if (value) {
            tryAddButton();
        } else {
            tryDeleteButton();
        }
    });

    SceneEvent().listen([](CCScene* scene) {
        toasty::ui::detail::applyFloatingButton(scene);
        return ListenerResult::Propagate;
    }).leak();
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



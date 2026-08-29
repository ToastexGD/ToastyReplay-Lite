#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#if !defined(GEODE_IS_IOS)
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#endif
#include "ToastyMenu.hpp"
#include "ui/Notifications.hpp"

using namespace geode::prelude;

$on_mod(Loaded) {
    if (Mod::get()->setSavedValue<bool>("migrated-open-key", true))
        return;

    if (Mod::get()->getSavedValue<int>("key-open-menu", static_cast<int>(KEY_T)) ==
        static_cast<int>(KEY_F8)) {
        Mod::get()->setSavedValue<int>("key-open-menu", static_cast<int>(KEY_T));
    }
}

class $modify(ToastyFirstRun, MenuLayer) {
    bool init() {
        if (!MenuLayer::init())
            return false;

        if (Mod::get()->setSavedValue<bool>("seen-open-hint-v2", true))
            return true;

#if defined(GEODE_IS_MOBILE)
        toasty::notifications::show("Open ToastyReplay-Lite from the pause menu, or using the floating button",
                                    NotificationIcon::Info);
#else
        toasty::notifications::show("Open ToastyReplay-Lite with the \"T\" keybind.",
                                    NotificationIcon::Info);
#endif

        if (auto saved = Mod::get()->saveData(); !saved)
            log::warn("could not save the first launch flag: {}", saved.unwrapErr());

        return true;
    }
};

// open menu keybind, no keyboard to hook on ios
#if !defined(GEODE_IS_IOS)
class $modify(ToastyKeys, CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool repeat, double timestamp) {
        if (ToastyMenu::handleKey(key, down, repeat))
            return true;
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, repeat, timestamp);
    }
};
#endif

class $modify(ToastyPause, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto menu = this->getChildByID("left-button-menu");
        if (!menu)
            return;

        // toasty menu button
        CCNode* spr = CCSprite::create("MenuIcon.png"_spr);
        if (!spr)
            spr = ButtonSprite::create("TR", "goldFont.fnt", "GJ_button_04.png", .8f);

        spr->setScale(34.5f / std::max(spr->getContentWidth(), spr->getContentHeight()));

        auto btn =
            CCMenuItemSpriteExtra::create(spr, this, menu_selector(ToastyPause::onToastyMenu));
        btn->setID("menu-button"_spr);
        menu->addChild(btn);
        menu->updateLayout();
    }

    void onToastyMenu(CCObject* sender) {
        ToastyMenu::create()->show();
    }
};

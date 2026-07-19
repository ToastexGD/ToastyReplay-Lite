#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include "ToastyMenu.hpp"

using namespace geode::prelude;

// open menu keybind
class $modify(ToastyKeys, CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool repeat, double timestamp) {
        if (down && !repeat && ToastyMenu::handleKey(key)) return true;
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, repeat, timestamp);
    }
};

class $modify(ToastyPause, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto menu = this->getChildByID("left-button-menu");
        if (!menu) return;

        // bot menu button
        CCNode* spr = CCSprite::create("MenuIcon.png"_spr);
        if (!spr) spr = ButtonSprite::create("TR", "goldFont.fnt", "GJ_button_04.png", .8f);

        spr->setScale(34.5f / std::max(spr->getContentWidth(), spr->getContentHeight()));

        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(ToastyPause::onToastyMenu));
        btn->setID("menu-button"_spr);
        menu->addChild(btn);
        menu->updateLayout();
    }

    void onToastyMenu(CCObject* sender) {
        ToastyMenu::create()->show();
    }
};

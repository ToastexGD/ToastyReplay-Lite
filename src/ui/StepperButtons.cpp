#include "StepperButtons.hpp"

#include "../timing/FrameStepper.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

namespace toasty::ui {
    bool stepperButtonsDefault() {
#if defined(GEODE_IS_MOBILE)
        return true;
#else
        return false;
#endif
    }

    bool stepperButtonsVisible() {
        return Mod::get()->getSavedValue<bool>("stepper-buttons", stepperButtonsDefault());
    }

    void refreshStepperButtons() {
        auto layer = PlayLayer::get();
        if (!layer) {
            return;
        }
        if (auto menu = layer->getChildByID("frame-stepper-menu"_spr)) {
            menu->setVisible(toasty::stepper::enabled() && stepperButtonsVisible());
        }
    }
} // namespace toasty::ui

class $modify(StepperPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        auto menu = CCMenu::create();
        menu->setID("frame-stepper-menu"_spr);
        menu->setPosition({0.f, 0.f});
        menu->setContentSize(winSize);

        auto stepSprite = ButtonSprite::create("Step", "bigFont.fnt", "GJ_button_01.png", .8f);
        stepSprite->setScale(.7f);
        auto stepButton =
            CCMenuItemSpriteExtra::create(stepSprite, this, menu_selector(StepperPlayLayer::onStep));
        stepButton->setID("step"_spr);
        stepButton->setPosition({winSize.width - 44.f, 34.f});
        menu->addChild(stepButton);

        this->addChild(menu, 100);
        toasty::ui::refreshStepperButtons();
        return true;
    }

    void onStep(CCObject* sender) {
        toasty::stepper::step();
    }
};

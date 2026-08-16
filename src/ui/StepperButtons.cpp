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
        auto menu = layer->getChildByIDRecursive("frame-stepper-menu"_spr);
        if (menu) {
            menu->setVisible(toasty::stepper::enabled() && stepperButtonsVisible());
        }
    }
} // namespace toasty::ui

class $modify(StepperPlayLayer, PlayLayer) {
    struct Fields {
        Ref<CCMenuItemSpriteExtra> stepButton;
    };

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
        m_fields->stepButton = stepButton;

        CCNode* host = m_uiLayer ? static_cast<CCNode*>(m_uiLayer) : static_cast<CCNode*>(this);
        host->addChild(menu, 100);
        handleTouchPriority(menu);

        toasty::ui::refreshStepperButtons();
        this->schedule(schedule_selector(StepperPlayLayer::pollStepButton));
        return true;
    }

    void pollStepButton(float dt) {
        auto button = m_fields->stepButton;
        auto parent = button ? button->getParent() : nullptr;
        toasty::stepper::setRepeating(button && parent && parent->isVisible() &&
                                      button->isSelected());
    }

    void onStep(CCObject* sender) {
        toasty::stepper::stepOnce();
    }
};

#include "StepperButtons.hpp"

#include "../engine/Engine.hpp"
#include "../timing/FrameStepper.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>
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
        if (!menu) {
            return;
        }
        menu->setVisible(toasty::stepper::sessionOpen() && stepperButtonsVisible());

        if (auto pause = typeinfo_cast<CCMenuItemSpriteExtra*>(
                menu->getChildByID("pause-stepper"_spr))) {
            auto frame = toasty::stepper::paused() ? "GJ_playEditorBtn_001.png"
                                                   : "GJ_pauseEditorBtn_001.png";
            if (auto sprite = CCSprite::createWithSpriteFrameName(frame)) {
                sprite->setScale(.5f);
                pause->setNormalImage(sprite);
                pause->updateSprite();
            }
        }

    }
} // namespace toasty::ui

class $modify(StepperPauseLayer, PauseLayer) {
    void onResume(CCObject* sender) {
        PauseLayer::onResume(sender);
        toasty::stepper::syncMusic();
        toasty::ui::refreshStepperButtons();
    }
};

class $modify(StepperPlayLayer, PlayLayer) {
    struct Fields {
        Ref<CCMenuItemSpriteExtra> stepButton;

    };

    CCMenuItemSpriteExtra* addStepperButton(CCMenu* menu,
                                            char const* frame,
                                            bool flip,
                                            std::string const& id,
                                            SEL_MenuHandler handler) {
        auto sprite = CCSprite::createWithSpriteFrameName(frame);
        if (!sprite) {
            return nullptr;
        }
        sprite->setFlipX(flip);
        sprite->setScale(.5f);
        auto button = CCMenuItemSpriteExtra::create(sprite, this, handler);
        button->setID(id);
        menu->addChild(button);
        return button;
    }

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        auto menu = CCMenu::create();
        menu->setID("frame-stepper-menu"_spr);
        menu->setPosition({0.f, 0.f});
        menu->setContentSize(winSize);

        auto pause = this->addStepperButton(menu,
                                            "GJ_pauseEditorBtn_001.png",
                                            false,
                                            "pause-stepper"_spr,
                                            menu_selector(StepperPlayLayer::onTogglePause));
        auto stop = this->addStepperButton(menu,
                                           "GJ_deleteSongBtn_001.png",
                                           false,
                                           "stop-stepper"_spr,
                                           menu_selector(StepperPlayLayer::onStopStepper));
        m_fields->stepButton = this->addStepperButton(menu,
                                                      "GJ_arrow_01_001.png",
                                                      true,
                                                      "step"_spr,
                                                      menu_selector(StepperPlayLayer::onStep));

        auto edge = winSize.width - 14.f;
        for (auto button : {m_fields->stepButton.data(), stop, pause}) {
            if (!button) {
                continue;
            }
            auto half = button->getScaledContentWidth() / 2.f;
            edge -= half;
            button->setPosition({edge, 30.f});
            edge -= half + 4.f;
        }

        CCNode* host = m_uiLayer ? static_cast<CCNode*>(m_uiLayer) : static_cast<CCNode*>(this);
        host->addChild(menu, 100);
        handleTouchPriority(menu);

        toasty::ui::refreshStepperButtons();
        this->schedule(schedule_selector(StepperPlayLayer::pollStepButton));
        return true;
    }

    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        toasty::stepper::closeSession();
        toasty::ui::refreshStepperButtons();
    }

    void onQuit() {
        toasty::stepper::closeSession();
        PlayLayer::onQuit();
    }

    void pollStepButton(float dt) {
        auto button = m_fields->stepButton;
        auto parent = button ? button->getParent() : nullptr;
        auto shown = parent && parent->isVisible();
        toasty::stepper::setButtonHeld(button && shown && button->isSelected());

    }

    void onStep(CCObject* sender) {
        toasty::stepper::stepOnce();
    }

    void onTogglePause(CCObject* sender) {
        toasty::stepper::setPaused(!toasty::stepper::paused());
        toasty::ui::refreshStepperButtons();
    }

    void onStopStepper(CCObject* sender) {
        toasty::stepper::closeSession();
        toasty::ui::refreshStepperButtons();
    }
};

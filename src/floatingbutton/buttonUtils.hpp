#include <Geode/Geode.hpp>
#include <fryy_55.amber/include/classes/DraggableButton.hpp>
#include <Geode/modify/MenuLayer.hpp>

#include "../ToastyMenu.hpp"

using namespace geode::prelude;


void tryAddButton() {
    auto ovM = OverlayManager::get();

    if (ovM->getChildByID("draggableMenuButton"_spr)) {
        log::info("couldnt add button. already exists!");
        return; 
    }


    auto sprite = CCSprite::create("MenuIcon.png"_spr);
    sprite->setScale(0.25f);
    auto btn = amber::DraggableButton::create(
        sprite,
        [](amber::DraggableButton* sender) static {
            auto value = Mod::get()->getSavedValue<bool>("menu-open");
            if (!value) {
                ToastyMenu::create()->show();
            }
            
        }
    );

    btn->setDragStartedCallback([btn, sprite](amber::DraggableButton* self) {
        btn->setOpacity(255);
        CCScaleTo* scaleDown = CCScaleTo::create(0.1f, 0.2f);
        CCEaseInOut* easedScale = CCEaseInOut::create(scaleDown, 1.2f);

        sprite->runAction(easedScale);

    });


    btn->setReleaseCallback([btn, sprite](amber::DraggableButton* self) {
        btn->setOpacity(150);
        CCScaleTo* scaleUp = CCScaleTo::create(0.1f, 0.25f);
        CCEaseInOut* easedScale = CCEaseInOut::create(scaleUp, 1.2f);
        Mod::get()->setSavedValue<float>("button-pos-x", btn->getPositionX());
        Mod::get()->setSavedValue<float>("button-pos-y", btn->getPositionY());
        Mod::get()->setSavedValue<bool>("dragged-before", true);
        sprite->runAction(easedScale);
    });

    btn->setDelay(0.3f);
    btn->setSnap(false);
    btn->setOpacity(150);
    btn->setID("draggableMenuButton"_spr);

    auto winSize = CCDirector::get()->getWinSize();
    auto contentSize = btn->getContentSize();

    CCPoint position;
    if (!Mod::get()->getSavedValue<bool>("dragged-before")) {
        position = CCPoint(contentSize.width * 3, winSize.height / 2);
    } else {
        position = CCPoint(Mod::get()->getSavedValue<float>("button-pos-x"), Mod::get()->getSavedValue<float>("button-pos-y"));
    }
    
    

    ovM->addChild(btn);
    btn->setPosition(position);
    
    if (!btn->setArea(amber::DraggableButton::Area::Screen)) {
        log::warn("Failed to set area for a button!");
    }

}

void tryDeleteButton() {
    auto ovM = OverlayManager::get();

    if (auto button = ovM->getChildByID("draggableMenuButton"_spr)) {
        button->removeFromParentAndCleanup(true);
        
    } else {
        log::info("couldnt remove button. button does not exist!");
        return; 
    }

}

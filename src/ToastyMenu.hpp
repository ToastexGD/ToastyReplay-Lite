#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/TextArea.hpp>
#include <vector>

using namespace geode::prelude;

class ToastyMenu : public geode::Popup {
protected:
    CCMenu* m_menu;
    int m_mode = 0;
    int m_tab = 0;
    CCScale9Sprite* m_modeCards[3] = {};
    CCSprite* m_modeIcons[3] = {};
    CCLabelBMFont* m_modeLabels[3] = {};
    CCScale9Sprite* m_tabBgs[6] = {};
    CCNode* m_pages[6] = {};
    std::vector<ScrollLayer*> m_scrolls[6];
    TextInput* m_seedInput;
    Slider* m_scaleSlider = nullptr;
    CCLabelBMFont* m_scalePct = nullptr;
    bool m_dragging = false;
    CCPoint m_dragOffset;

    bool init();
    ~ToastyMenu();
    CCNode* makeMacroCell(std::string const& name);
    void updateModeCards();
    void updateTabs();
    void updatePages();
    void clampMainLayer();

    void onMode(CCObject* sender);
    void onTab(CCObject* sender);
    void onNothing(CCObject* sender);
    void onRename(CCObject* sender);
    void onSeedInfo(CCObject* sender);
    void onScaleSlider(CCObject* sender);
    void onBindKey(CCObject* sender);
    void onClose(CCObject* sender) override;

    bool ccTouchBegan(CCTouch* touch, CCEvent* event) override;
    void ccTouchMoved(CCTouch* touch, CCEvent* event) override;
    void ccTouchEnded(CCTouch* touch, CCEvent* event) override;

public:
    static ToastyMenu* create();
    static bool handleKey(enumKeyCodes key);
    void show() override;
};

class RenamePopup : public geode::Popup {
protected:
    CCLabelBMFont* m_target;
    TextInput* m_input;

    bool init(CCLabelBMFont* target);
    void onSave(CCObject* sender);

public:
    static RenamePopup* create(CCLabelBMFont* target);
};

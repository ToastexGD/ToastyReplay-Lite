#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/Scrollbar.hpp>
#include <Geode/ui/TextArea.hpp>
#include <vector>

using namespace geode::prelude;

class ToastyMenu : public geode::Popup {
protected:
    enum Tab { TabMain, TabMacros, TabSettings, TabKeybinds, TabAbout, TabCount };
    struct Group {
        CCNode* node = nullptr;
        CCMenu* menu = nullptr;
    };
    int m_mode = 0;
    int m_tab = TabMain;
    NineSlice* m_modeBgs[3] = {};
    CCLabelBMFont* m_modeLabels[3] = {};
    NineSlice* m_tabBgs[TabCount] = {};
    CCNode* m_pages[TabCount] = {};
    std::vector<CCLayer*> m_pageTouchNodes[TabCount];
    TextInput* m_seedInput = nullptr;
    TextInput* m_tpsInput = nullptr;
    CCMenuItemToggler* m_tpsToggle = nullptr;
    Slider* m_scaleSlider = nullptr;
    CCLabelBMFont* m_scalePct = nullptr;
    bool m_dragging = false;
    CCPoint m_dragOffset;

    bool init() override;
    ~ToastyMenu();
    void addHeader();
    void addSidebar();
    Group makePage(int tab);
    void addPageTitle(CCNode* page, const char* title, const char* hint);
    NineSlice* addPanel(CCNode* page, CCPoint center, CCSize size);
    ScrollLayer* addScroll(CCNode* page, int tab, CCPoint pos, CCSize size);

    Group makeRow(const char* title, float height, float titleWidth);
    CCNode* makeToggleRow(const char* id, const char* title, bool on);
    CCNode* makeTpsRow();
    CCNode* makeSectionRow(const char* title);
    CCNode* makeMacroRow(const char* name);
    CCNode* makeKeybindRow(const char* title, const char* saveId, enumKeyCodes def);
    void updateModes();
    void updateTabs();
    void updatePages();
    void clampMainLayer();

    void onMode(CCObject* sender);
    void onTab(CCObject* sender);
    void onToggleOption(CCObject* sender);
    void onTpsToggle(CCObject* sender);
    void onTpsAdjust(CCObject* sender);
    void onTpsInfo(CCObject* sender);
    void onAddMacro(CCObject* sender);
    void onRefreshMacros(CCObject* sender);
    void onMacroOptions(CCObject* sender);
    void onAccentPrev(CCObject* sender);
    void onAccentNext(CCObject* sender);
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

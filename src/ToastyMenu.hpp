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
    friend class RenameMacroPopup;

  protected:
    enum Tab { TabMain, TabMacros, TabSettings, TabKeybinds, TabAbout, TabCount };
    struct Group {
        CCNode* node = nullptr;
        CCMenu* menu = nullptr;
    };
    int m_tab = TabMain;
    NineSlice* m_modeBgs[3] = {};
    CCLabelBMFont* m_modeLabels[3] = {};
    NineSlice* m_tabBgs[TabCount] = {};
    CCNode* m_pages[TabCount] = {};
    std::vector<CCLayer*> m_pageTouchNodes[TabCount];
    ScrollLayer* m_macroScroll = nullptr;
    std::vector<std::string> m_macroNames;
    std::vector<NineSlice*> m_macroRowBgs;
    int m_selectedMacro = -1;
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
    void addPageTitle(CCNode* page, ZStringView title, ZStringView hint = {});
    NineSlice* addPanel(CCNode* page, CCPoint center, CCSize size);
    ScrollLayer* addScroll(CCNode* page, int tab, CCPoint pos, CCSize size);

    Group makeRow(ZStringView title, float height, float titleWidth);
    CCNode* makeToggleRow(ZStringView id, ZStringView title, bool on);
    CCNode* makeTpsRow();
    CCNode* makeSectionRow(ZStringView title);
    CCNode* makeMacroRow(std::string const& fileName, int index);
    CCNode* makeKeybindRow(ZStringView title, ZStringView saveId, enumKeyCodes def);
    void updateModes();
    void updateTabs();
    void updatePages();
    void clampMainLayer();

    void refreshMacroList();
    void onMode(CCObject* sender);
    void onTab(CCObject* sender);
    void onSelectMacro(CCObject* sender);
    void onToggleOption(CCObject* sender);
    void onTpsToggle(CCObject* sender);
    void onTpsAdjust(CCObject* sender);
    void onTpsInfo(CCObject* sender);
    void onRefreshMacros(CCObject* sender);
    void onReplayMacro(CCObject* sender);
    void onRenameMacro(CCObject* sender);
    void onDeleteMacro(CCObject* sender);
    void onAccentPrev(CCObject* sender);
    void onAccentNext(CCObject* sender);
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

class RenameMacroPopup : public geode::Popup {
  protected:
    WeakRef<ToastyMenu> m_menu;
    std::string m_fileName;
    TextInput* m_input = nullptr;

    bool init(ToastyMenu* menu, std::string fileName);
    void onOpenFolder(CCObject* sender);
    void onSave(CCObject* sender);

  public:
    static RenameMacroPopup* create(ToastyMenu* menu, std::string fileName);
};

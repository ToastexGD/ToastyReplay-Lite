#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/Scrollbar.hpp>
#include <Geode/ui/SliderNode.hpp>
#include <Geode/ui/TextArea.hpp>
#include <filesystem>
#include <optional>
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
    struct MacroInfo {
        bool loaded = false;
        size_t inputs = 0;
        size_t frameFixes = 0;
        uint64_t tickCount = 0;
        double tps = 0.0;
        double duration = 0.0;
        uint32_t gameVersion = 0;
        uint64_t levelId = 0;
        uint64_t levelRevision = 0;
        bool platformer = false;
        std::optional<uint64_t> seed;
        std::optional<float> startPos;
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
    MacroInfo m_macroInfo;
    std::string m_macroInfoName;
    CCLabelBMFont* m_macroInfoLabel = nullptr;
    CCMenuItemSpriteExtra* m_macroInfoButton = nullptr;
    float m_macroInfoLeft = 0.f;
    float m_macroInfoRight = 0.f;
    float m_macroInfoRightWide = 0.f;
    std::optional<std::filesystem::file_time_type> m_macroDirTime;
    TextInput* m_seedInput = nullptr;
    TextInput* m_speedInput = nullptr;
    CCMenuItemToggler* m_speedToggle = nullptr;
    TextInput* m_tpsInput = nullptr;
    CCMenuItemToggler* m_tpsToggle = nullptr;
    ccColor3B m_accentColor = {0, 110, 60};
    NineSlice* m_accentSwatch = nullptr;
    SliderNode* m_scaleSlider = nullptr;
    CCLabelBMFont* m_scalePct = nullptr;
    bool m_dragging = false;
    CCPoint m_dragOffset;

    bool init() override;
    ~ToastyMenu();
    void addHeader();
    void addSidebar();
    void addSocialButton(CCMenu* menu, CCSprite* icon, ZStringView id, ZStringView url);
    Group makePage(int tab);
    void addPageTitle(CCNode* page, ZStringView title, ZStringView hint = {});
    NineSlice* addPanel(CCNode* page, CCPoint center, CCSize size);
    ScrollLayer* addScroll(CCNode* page, int tab, CCPoint pos, CCSize size);

    Group makeRow(ZStringView title, float height, float titleWidth, bool enabled = true);
    CCNode* makeToggleRow(ZStringView id, ZStringView title, bool on, bool enabled = true);
    CCNode* makeSeedRow();
    CCNode* makeSpeedhackRow();
    CCNode* makeTpsRow();
    CCNode* makeFrameStepperRow();
    CCNode* makeNoclipRow();
    CCNode* makeSectionRow(ZStringView title);
    CCNode* makeMacroRow(std::string const& fileName, int index);
    CCNode* makeKeybindRow(ZStringView title, ZStringView saveId, enumKeyCodes def);
    void updateModes();
    void updateTabs();
    void updatePages();
    void clampMainLayer();

    void refreshMacroList(bool keepScroll = false);
    void checkMacroDirectory(float dt);
    void updateMacroInfo();
    void onMacroInfo(CCObject* sender);
    void onMode(CCObject* sender);
    void onTab(CCObject* sender);
    void onSelectMacro(CCObject* sender);
    void onToggleOption(CCObject* sender);
    void onSpeedhackToggle(CCObject* sender);
    void onSpeedhackAdjust(CCObject* sender);
    void onSpeedhackOptions(CCObject* sender);
    void onSeedAdjust(CCObject* sender);
    void onTpsToggle(CCObject* sender);
    void onTpsAdjust(CCObject* sender);
    void onTpsInfo(CCObject* sender);
    void onFrameStepperToggle(CCObject* sender);
    void onFrameStepperOptions(CCObject* sender);
    void onNoclipOptions(CCObject* sender);
    void onSeedInfo(CCObject* sender);
    void onReplayMacro(CCObject* sender);
    void onMacroOptions(CCObject* sender);
    void onDeleteMacro(CCObject* sender);
    static void finishAddMacroFile(std::optional<std::filesystem::path> picked);
    void onAddMacroFile(CCObject* sender);
    void onOpenFolder(CCObject* sender);
    void onSocialLink(CCObject* sender);
    void onAccentColor(CCObject* sender);
    void setAccentColor(ccColor3B color);
    void onBindKey(CCObject* sender);
    void onClose(CCObject* sender) override;

    bool ccTouchBegan(CCTouch* touch, CCEvent* event) override;
    void ccTouchMoved(CCTouch* touch, CCEvent* event) override;
    void ccTouchEnded(CCTouch* touch, CCEvent* event) override;

  public:
    static ToastyMenu* create();
    static bool isOpen();
    static bool handleKey(enumKeyCodes key, bool down, bool repeat);
    void show() override;
};

class OptionsPopup : public geode::Popup {
  public:
    struct Option {
        std::string id;
        std::string title;
        bool defaultValue = false;
        std::string info;
    };

    static OptionsPopup* create(std::string title, std::vector<Option> options);

  protected:
    bool init(std::string title, std::vector<Option> options);
    void onToggle(CCObject* sender);
    void onInfo(CCObject* sender);
};

class RenameMacroPopup : public geode::Popup {
  protected:
    WeakRef<ToastyMenu> m_menu;
    std::string m_fileName;
    TextInput* m_input = nullptr;

    bool init(ToastyMenu* menu, std::string fileName);
    void onSave(CCObject* sender);

  public:
    static RenameMacroPopup* create(ToastyMenu* menu, std::string fileName);
};

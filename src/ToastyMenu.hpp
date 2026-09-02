#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/ui/Label.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/Scrollbar.hpp>
#include <Geode/ui/SliderNode.hpp>
#include <Geode/ui/TextArea.hpp>
#include <filesystem>
#include <optional>
#include <vector>

class ToastyMenu : public geode::Popup {
    friend class RenameMacroPopup;

  protected:
    enum Tab { TabMain, TabMacros, TabSettings, TabKeybinds, TabAbout, TabCount };
    struct Group {
        cocos2d::CCNode* node = nullptr;
        cocos2d::CCMenu* menu = nullptr;
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
    // geode::NineSlice* m_modeBgs[3] = {};
    std::array<geode::NineSlice*, 3> m_modeBgs{};
    std::array<geode::Label*, 3> m_modeLabels{};
    std::array<geode::NineSlice*, TabCount> m_tabBgs{};
    std::array<cocos2d::CCNode*, TabCount> m_pages{};
    std::array<std::vector<geode::Ref<cocos2d::CCLayer>>, TabCount> m_pageTouchNodes{};
    geode::ScrollLayer* m_macroScroll = nullptr;
    std::vector<std::string> m_macroNames;
    std::vector<geode::NineSlice*> m_macroRowBgs;
    int m_selectedMacro = -1;
    MacroInfo m_macroInfo;
    std::string m_macroInfoName;
    geode::Label* m_macroInfoLabel = nullptr;
    CCMenuItemSpriteExtra* m_macroInfoButton = nullptr;
    float m_macroInfoLeft = 0.f;
    float m_macroInfoRight = 0.f;
    float m_macroInfoRightWide = 0.f;
    std::optional<std::filesystem::file_time_type> m_macroDirTime;
    geode::TextInput* m_seedInput = nullptr;
    geode::TextInput* m_speedInput = nullptr;
    CCMenuItemToggler* m_speedToggle = nullptr;
    geode::TextInput* m_tpsInput = nullptr;
    CCMenuItemToggler* m_tpsToggle = nullptr;
    cocos2d::ccColor3B m_accentColor = {0, 110, 60};
    geode::NineSlice* m_accentSwatch = nullptr;
    geode::SliderNode* m_scaleSlider = nullptr;
    geode::Label* m_scalePct = nullptr;
    bool m_dragging = false;
    cocos2d::CCPoint m_dragOffset;

    bool init() override;
    ~ToastyMenu();
    void addHeader();
    void addSidebar();
    void addSocialButton(cocos2d::CCMenu* menu, cocos2d::CCSprite* icon, geode::ZStringView id, geode::ZStringView url);
    Group makePage(int tab);
    void addPageTitle(cocos2d::CCNode* page, geode::ZStringView title, geode::ZStringView hint = {});
    geode::NineSlice* addPanel(cocos2d::CCNode* page, cocos2d::CCPoint center, cocos2d::CCSize size);
    geode::ScrollLayer* addScroll(cocos2d::CCNode* page, int tab, cocos2d::CCPoint pos, cocos2d::CCSize size);

    Group makeRow(geode::ZStringView title, float height, float titleWidth, bool enabled = true);
    cocos2d::CCNode* makeToggleRow(geode::ZStringView id, geode::ZStringView title, bool on, bool enabled = true);
    cocos2d::CCNode* makeSeedRow();
    cocos2d::CCNode* makeSpeedhackRow();
    cocos2d::CCNode* makeTpsRow();
    cocos2d::CCNode* makeFrameStepperRow();
    cocos2d::CCNode* makeNoclipRow();
    cocos2d::CCNode* makeSectionRow(geode::ZStringView title);
    cocos2d::CCNode* makeMacroRow(std::string const& fileName, int index);
    cocos2d::CCNode* makeKeybindRow(geode::ZStringView title, geode::ZStringView saveId, cocos2d::enumKeyCodes def);
    void updateModes();
    void updateTabs();
    void updatePages();
    void clampMainLayer();

    void refreshMacroList(bool keepScroll = false);
    void checkMacroDirectory(float dt);
    void updateMacroInfo();
    void onMacroInfo(cocos2d::CCObject* sender);
    void onMode(cocos2d::CCObject* sender);
    void onTab(cocos2d::CCObject* sender);
    void onSelectMacro(cocos2d::CCObject* sender);
    void onToggleOption(cocos2d::CCObject* sender);
    void onSpeedhackToggle(cocos2d::CCObject* sender);
    void onSpeedhackAdjust(cocos2d::CCObject* sender);
    void onSpeedhackOptions(cocos2d::CCObject* sender);
    void onSeedAdjust(cocos2d::CCObject* sender);
    void onTpsToggle(cocos2d::CCObject* sender);
    void onTpsAdjust(cocos2d::CCObject* sender);
    void onTpsInfo(cocos2d::CCObject* sender);
    void onFrameStepperToggle(cocos2d::CCObject* sender);
    void onFrameStepperOptions(cocos2d::CCObject* sender);
    void onNoclipOptions(cocos2d::CCObject* sender);
    void onSeedInfo(cocos2d::CCObject* sender);
    void onReplayMacro(cocos2d::CCObject* sender);
    void onRenameMacro(cocos2d::CCObject* sender);
    void onDeleteMacro(cocos2d::CCObject* sender);
    void onAddMacroFile(cocos2d::CCObject* sender);
    void onOpenFolder(cocos2d::CCObject* sender);
    void onSocialLink(cocos2d::CCObject* sender);
    void onAccentColor(cocos2d::CCObject* sender);
    void setAccentColor(cocos2d::ccColor3B color);
    void onBindKey(cocos2d::CCObject* sender);
    void onClose(cocos2d::CCObject* sender) override;

    bool ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
    void ccTouchMoved(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
    void ccTouchEnded(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;

  public:
    static void finishAddMacroFile(std::optional<std::filesystem::path> picked);
    static ToastyMenu* create();
    static bool isOpen();
    static bool handleKey(cocos2d::enumKeyCodes key, bool down, bool repeat);
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
    void onToggle(cocos2d::CCObject* sender);
    void onInfo(cocos2d::CCObject* sender);
};

class RenameMacroPopup : public geode::Popup {
  protected:
    geode::WeakRef<ToastyMenu> m_menu;
    std::string m_fileName;
    geode::TextInput* m_input = nullptr;

    bool init(ToastyMenu* menu, std::string fileName);
    void onSave(cocos2d::CCObject* sender);

  public:
    static RenameMacroPopup* create(ToastyMenu* menu, std::string fileName);
};

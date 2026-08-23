#include "ToastyMenu.hpp"
#include <arc/future/Future.hpp>
#include <asp/fs.hpp>
#include <asp/iter.hpp>
#include <fmt/ranges.h>
#include <Geode/ui/ColorPickPopup.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/web.hpp>
#include "engine/Engine.hpp"
#include "engine/RandomSeed.hpp"
#include "replay/TtrlStorage.hpp"
#include "timing/FrameStepper.hpp"
#include "timing/Speedhack.hpp"
#include "timing/TpsBypass.hpp"
#include "ui/Notifications.hpp"
#include "ui/StepperButtons.hpp"
#include "ui/Watermark.hpp"

#include <algorithm>

static constexpr float POPUP_W = 450.f;
static constexpr float POPUP_H = 290.f;
static constexpr float MARGIN = 12.f;
static constexpr float GAP = 10.f;
static constexpr float HEADER_Y = 272.f;
static constexpr float HEADER_ICON = 22.f;
static constexpr float DIVIDER_Y = 256.f;
static constexpr float CONTENT_TOP = 236.f;
static constexpr float CONTENT_BOTTOM = MARGIN;
static constexpr float FOOTER_H = 30.f;
static constexpr float FOOTER_Y = CONTENT_BOTTOM + FOOTER_H / 2.f;
static constexpr float COLUMN_BOTTOM = CONTENT_BOTTOM + FOOTER_H + GAP;
static constexpr float SIDE_X = 58.f;
static constexpr float SIDE_W = 92.f;
static constexpr float TAB_W = 74.f;
static constexpr float TAB_H = 26.f;
static constexpr float TAB_GAP = 10.f;
static constexpr float TABS_H = TAB_H * 5.f + TAB_GAP * 4.f;
static constexpr float SIDE_PAD = (CONTENT_TOP - COLUMN_BOTTOM - TABS_H) / 2.f;
static constexpr float SOCIAL_ICON = 26.f;
static constexpr float SOCIAL_Y = FOOTER_Y;
static constexpr float PANEL_X = 269.f;
static constexpr float PANEL_W = 314.f;
static constexpr float PANEL_LEFT = PANEL_X - PANEL_W / 2.f;
static constexpr float PANEL_RIGHT = PANEL_X + PANEL_W / 2.f;
static constexpr float PANEL_PAD = 4.f;
static constexpr float SCROLLBAR_X = 434.f;
static constexpr float TITLE_Y = 246.f;
static constexpr float MAIN_PANEL_TOP = 186.f;
static constexpr float ROW_X = 116.f;
static constexpr float ROW_W = 306.f;
static constexpr float ROW_H = 26.f;
static constexpr float ROW_PAD = 10.f;
static constexpr float SWATCH_SIZE = 20.f;
static constexpr float CTRL_GAP = 8.f;
static constexpr float MACRO_ROW_H = 30.f;
static constexpr float FOOTER_ICON = 24.f;
static constexpr float FOOTER_INFO_ICON = 16.f;
static constexpr float FOOTER_TEXT_SCALE = .32f;
static constexpr ccColor3B PANEL_COLOR = {58, 29, 13};
static constexpr ccColor3B DEFAULT_ACCENT_COLOR = {0, 110, 60};
static constexpr float ROW_CONTROL_RIGHT = ROW_W - ROW_PAD;
static constexpr float CONTROL_CENTER = ROW_CONTROL_RIGHT - SWATCH_SIZE / 2.f;

static void
moveCloseTopRight(CCMenuItemSpriteExtra* closeBtn, CCNode* mainLayer, CCSize const& size) {
    closeBtn->setPosition(closeBtn->getParent()->convertToNodeSpace(
        mainLayer->convertToWorldSpace({size.width - 8.f, size.height - 8.f})));
}

static geode::NineSlice* makeBG(CCSize size, ccColor3B color, GLubyte opacity, bool soft) {
    auto bg = geode::NineSlice::create("square02b_001.png");
    if (soft) {
        bg->setContentSize({size.width * 2.f, size.height * 2.f});
        bg->setScale(.5f);
    } else {
        bg->setContentSize(size);
    }
    bg->setColor(color);
    bg->setOpacity(opacity);
    return bg;
}

static float placeRight(CCNode* node, float right, float y) {
    auto width = node->getScaledContentWidth();
    node->setPosition({right - width / 2.f, y});
    return right - width - CTRL_GAP;
}

static float placeCenter(CCNode* node, float center, float y) {
    node->setPosition({center, y});
    return center - node->getScaledContentWidth() / 2.f - CTRL_GAP;
}

static float placeLeft(CCNode* node, float left, float y) {
    auto width = node->getScaledContentWidth();
    node->setPosition({left + width / 2.f, y});
    return left + width + CTRL_GAP;
}

static CCNode* makeTextRow(std::string text, float scale) {
    auto row = CCNode::create();
    auto area = SimpleTextArea::create(std::move(text), "chatFont.fnt", scale, ROW_W - 20.f);
    area->setAnchorPoint({0.f, 1.f});
    row->setContentSize({ROW_W, area->getHeight() + 10.f});
    area->setPosition({10.f, row->getContentHeight() - 5.f});
    row->addChild(area);
    return row;
}

static ToastyMenu* s_instance = nullptr;
static ButtonSprite* s_captureBtn = nullptr;
static std::string s_captureId;
static std::string s_capturePrev;
static constexpr auto MACRO_FILE_ID = "toastexgd.toastyreplay-lite/macro-file";

static void resumeIfPaused() {
    auto scene = CCDirector::sharedDirector()->getRunningScene();
    if (!scene)
        return;
    if (auto pause = scene->getChildByType<PauseLayer>(0)) {
        pause->onResume(nullptr);
    }
}

static void startRecordingFromMenu() {
    if (toasty::engine::startRecording()) {
        resumeIfPaused();
    }
}

static void startReplayFromMenu(std::string name) {
    if (toasty::engine::loadReplayAndBegin(std::move(name))) {
        resumeIfPaused();
    }
}

static void setInteraction(CCNode* node, bool enabled) {
    if (auto menu = typeinfo_cast<CCMenu*>(node)) {
        menu->setTouchEnabled(enabled);
    }
    if (auto scroll = typeinfo_cast<ScrollLayer*>(node)) {
        scroll->setTouchEnabled(enabled);
    }
    if (!node->getChildren())
        return;
    for (auto child : CCArrayExt<CCNode*>(node->getChildren())) {
        setInteraction(child, enabled);
    }
}

static std::string macroNameFromSender(CCObject* sender) {
    auto node = typeinfo_cast<CCNode*>(sender);
    auto name = node ? typeinfo_cast<CCString*>(node->getUserObject(MACRO_FILE_ID)) : nullptr;
    return name ? std::string(name->getCString()) : std::string();
}

static void setMacroName(CCNode* node, std::string const& fileName) {
    node->setUserObject(MACRO_FILE_ID, CCString::create(fileName));
}

static std::string keyName(enumKeyCodes key) {
    std::string name = CCKeyboardDispatcher::get()->keyToString(key);
    return name.empty() ? "?" : name;
}

static std::string speedText(double speed) {
    auto value = fmt::format("{:.3f}", speed);
    while (value.size() > 2 && value.back() == '0' && value[value.size() - 2] != '.') {
        value.pop_back();
    }
    return value;
}

static std::optional<std::filesystem::file_time_type> macroDirectoryTime() {
    auto time = asp::fs::lastWriteTime(toasty::replay::ttrl::defaultReplayDirectory());
    if (time.isErr()) {
        return std::nullopt;
    }
    return time.unwrap();
}

static std::string durationText(double seconds) {
    if (seconds <= 0.0) {
        return "0:00.00";
    }
    auto total = static_cast<uint64_t>(seconds);
    auto hundredths = static_cast<int>((seconds - static_cast<double>(total)) * 100.0);
    return fmt::format("{}:{:02}.{:02}", total / 60, total % 60, hundredths);
}

static std::string gameVersionText(uint32_t version) {
    if (version == 0) {
        return "Unknown";
    }
    return fmt::format("{}.{:04}", version / 10000, version % 10000);
}

static std::string modVersion() {
    return Mod::get()->getVersion().toVString();
}

static std::string modDevelopers() {
    auto devs = Mod::get()->getDevelopers();
    if (devs.empty())
        return "Unknown";
    return fmt::format("{}", fmt::join(devs, ", "));
}

ToastyMenu* ToastyMenu::create() {
    auto ret = new ToastyMenu();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool ToastyMenu::isOpen() {
    return s_instance != nullptr;
}

ToastyMenu::~ToastyMenu() {
    if (s_instance == this) {
        s_instance = nullptr;
        s_captureBtn = nullptr;
    }
}

bool ToastyMenu::init() {
    if (!Popup::init(POPUP_W, POPUP_H))
        return false;

    m_accentColor =
        Mod::get()->getSavedValue<ccColor3B>("accent-color", DEFAULT_ACCENT_COLOR);

    // top right x
    moveCloseTopRight(m_closeBtn, m_mainLayer, m_size);

    this->addHeader();
    this->addSidebar();

    // main page
    {
        auto [page, menu] = this->makePage(TabMain);
        this->addPageTitle(page, "Macro Controls", nullptr);

        // disable, record, play, buttons in the "macro controlls"
        const char* modeNames[3] = {"Disable", "Record", "Play"};
        const char* modeTextures[3] = {"GJ_button_04.png", "GJ_button_06.png", "GJ_button_01.png"};
        const char* modeIds[3] = {"mode-disable", "mode-record", "mode-play"};
        for (int i = 0; i < 3; i++) {
            auto node = CCNode::create();
            node->setContentSize({98.f, 40.f});

            auto bg = geode::NineSlice::create(modeTextures[i]);
            bg->setContentSize(node->getContentSize());
            bg->setPosition({49.f, 20.f});
            node->addChild(bg);
            m_modeBgs[i] = bg;

            auto label = CCLabelBMFont::create(modeNames[i], "bigFont.fnt");
            label->setPosition({49.f, 20.f});
            label->limitLabelWidth(78.f, .5f, .1f);
            node->addChild(label);
            m_modeLabels[i] = label;

            auto item =
                CCMenuItemSpriteExtra::create(node, this, menu_selector(ToastyMenu::onMode));
            item->setTag(i);
            item->setID(modeIds[i]);
            item->setPosition({PANEL_LEFT + 49.f + i * 108.f, MAIN_PANEL_TOP + GAP + 20.f});
            menu->addChild(item);
        }

        this->addPanel(page, {PANEL_X, (MAIN_PANEL_TOP + CONTENT_BOTTOM) / 2.f}, {PANEL_W, MAIN_PANEL_TOP - CONTENT_BOTTOM});
        auto scroll = this->addScroll(page, TabMain, {ROW_X, CONTENT_BOTTOM + PANEL_PAD}, {ROW_W, MAIN_PANEL_TOP - CONTENT_BOTTOM - PANEL_PAD * 2.f});

        scroll->m_contentLayer->addChild(this->makeNoclipRow());
        scroll->m_contentLayer->addChild(this->makeTpsRow());
        scroll->m_contentLayer->addChild(this->makeSpeedhackRow());
        scroll->m_contentLayer->addChild(this->makeFrameStepperRow());
        scroll->m_contentLayer->addChild(this->makeSeedRow());
        scroll->m_contentLayer->addChild(
            this->makeToggleRow("safe-mode",
                                "Safe Mode",
                                Mod::get()->getSavedValue<bool>("safe-mode", false)));

        scroll->m_contentLayer->updateLayout();
        scroll->scrollToTop();
        this->updateModes();
    }

    // macros page
    {
        auto page = this->makePage(TabMacros).node;
        this->addPageTitle(page, "Macro List", nullptr);

        this->addPanel(page, {PANEL_X, (CONTENT_TOP + COLUMN_BOTTOM) / 2.f}, {PANEL_W, CONTENT_TOP - COLUMN_BOTTOM});

        float footerY = FOOTER_Y;

        auto footer = makeBG({PANEL_W, FOOTER_H}, PANEL_COLOR, 220, false);
        footer->setPosition({PANEL_X, footerY});
        page->addChild(footer);

        auto footerMenu = CCMenu::create();
        footerMenu->setPosition({0.f, 0.f});
        footerMenu->setContentSize(m_size);
        footerMenu->setID("macro-footer");
        page->addChild(footerMenu);

        float scrollBottom = COLUMN_BOTTOM + PANEL_PAD;
        m_macroScroll = this->addScroll(page,
                                        TabMacros,
                                        {ROW_X, scrollBottom},
                                        {ROW_W, CONTENT_TOP - PANEL_PAD - scrollBottom});

        float footerNext = PANEL_LEFT + ROW_PAD;

        if (auto plusSpr = CCSprite::createWithSpriteFrameName("GJ_plusBtn_001.png")) {
            plusSpr->setScale(FOOTER_ICON /
                              std::max(plusSpr->getContentWidth(), plusSpr->getContentHeight()));
            auto addBtn = CCMenuItemSpriteExtra::create(
                plusSpr, this, menu_selector(ToastyMenu::onAddMacroFile));
            addBtn->setID("add-from-file");
            footerNext = placeLeft(addBtn, footerNext, footerY);
            footerMenu->addChild(addBtn);
        }

        m_macroInfoLeft = footerNext;

        auto replaySpr = ButtonSprite::create("Replay", "bigFont.fnt", "GJ_button_01.png", .8f);
        replaySpr->setScale(.6f);
        auto replayBtn = CCMenuItemSpriteExtra::create(
            replaySpr, this, menu_selector(ToastyMenu::onReplayMacro));
        replayBtn->setID("replay");
        m_macroInfoRightWide = placeRight(replayBtn, PANEL_RIGHT - ROW_PAD, footerY);
        m_macroInfoRight = m_macroInfoRightWide;
        footerMenu->addChild(replayBtn);

        if (auto infoSpr = CCSprite::createWithSpriteFrameName("GJ_infoIcon_001.png")) {
            infoSpr->setScale(FOOTER_INFO_ICON /
                              std::max(infoSpr->getContentWidth(), infoSpr->getContentHeight()));
            m_macroInfoButton = CCMenuItemSpriteExtra::create(
                infoSpr, this, menu_selector(ToastyMenu::onMacroInfo));
            m_macroInfoButton->setID("macro-info");
            m_macroInfoRight = placeRight(m_macroInfoButton, m_macroInfoRightWide, footerY);
            footerMenu->addChild(m_macroInfoButton);
        }

        m_macroInfoLabel = CCLabelBMFont::create("", "bigFont.fnt");
        m_macroInfoLabel->setAnchorPoint({1.f, .5f});
        m_macroInfoLabel->setPosition({m_macroInfoRight, footerY});
        m_macroInfoLabel->setID("macro-info-label");
        page->addChild(m_macroInfoLabel);

        this->refreshMacroList();
    }

    // settings page
    {
        auto page = this->makePage(TabSettings).node;
        this->addPageTitle(page, "Settings", nullptr);

        this->addPanel(page, {PANEL_X, (CONTENT_TOP + CONTENT_BOTTOM) / 2.f}, {PANEL_W, CONTENT_TOP - CONTENT_BOTTOM});
        auto scroll = this->addScroll(page, TabSettings, {ROW_X, CONTENT_BOTTOM + PANEL_PAD}, {ROW_W, CONTENT_TOP - CONTENT_BOTTOM - PANEL_PAD * 2.f});

        // menu scale is live so the popup can be sized before anything else exists
        auto scaleRow = this->makeRow("Menu Scale", ROW_H, 95.f);
        scaleRow.node->setID("menu-scale");

        float savedScale = Mod::get()->getSavedValue<float>("menu-scale", 1.f);

        m_scaleSlider = Slider::create(this, menu_selector(ToastyMenu::onScaleSlider), .45f);
        m_scaleSlider->setPosition({185.f, ROW_H / 2.f});
        m_scaleSlider->setValue((savedScale - .7f) / .4f);
        scaleRow.node->addChild(m_scaleSlider);

        m_scalePct = CCLabelBMFont::create(
            fmt::format("{}%", static_cast<int>(std::round(savedScale * 100.f))).c_str(),
            "bigFont.fnt");
        m_scalePct->setAnchorPoint({1.f, .5f});
        m_scalePct->setScale(.35f);
        m_scalePct->setPosition({ROW_W - 10.f, ROW_H / 2.f});
        scaleRow.node->addChild(m_scalePct);
        scroll->m_contentLayer->addChild(scaleRow.node);

        auto accentRow = this->makeRow("Accent Color", ROW_H, 95.f);
        accentRow.node->setID("accent-color");

        m_accentSwatch = makeBG({SWATCH_SIZE, SWATCH_SIZE}, m_accentColor, 255, true);
        auto accentButton = CCMenuItemSpriteExtra::create(
            m_accentSwatch, this, menu_selector(ToastyMenu::onAccentColor));
        accentButton->setID("picker");
        placeCenter(accentButton, CONTROL_CENTER, ROW_H / 2.f);
        accentRow.menu->addChild(accentButton);
        scroll->m_contentLayer->addChild(accentRow.node);

        scroll->m_contentLayer->addChild(
            this->makeToggleRow(
                "show-notifications",
                "Show Notifications",
                Mod::get()->getSavedValue<bool>("show-notifications", true)));
        scroll->m_contentLayer->addChild(
            this->makeToggleRow("auto-save-macros",
                                "Auto Save Macros",
                                Mod::get()->getSavedValue<bool>("auto-save-macros", true)));

        scroll->m_contentLayer->updateLayout();
        scroll->scrollToTop();

        auto folderMenu = CCMenu::create();
        folderMenu->setPosition({0.f, 0.f});
        folderMenu->setContentSize(m_size);
        folderMenu->setID("settings-footer");
        page->addChild(folderMenu);

        if (auto folderSpr = CCSprite::createWithSpriteFrameName("gj_folderBtn_001.png")) {
            folderSpr->setScale(.7f);
            auto folderButton = CCMenuItemSpriteExtra::create(
                folderSpr, this, menu_selector(ToastyMenu::onOpenFolder));
            folderButton->setID("open-folder");
            placeRight(folderButton,
                       PANEL_RIGHT - ROW_PAD,
                       CONTENT_BOTTOM + ROW_PAD + folderButton->getScaledContentHeight() / 2.f);
            folderMenu->addChild(folderButton);
        }
    }

    // keybinds page
    {
        auto page = this->makePage(TabKeybinds).node;
        this->addPageTitle(page, "Keybinds", "Windows & macOS");

        this->addPanel(page, {PANEL_X, (CONTENT_TOP + CONTENT_BOTTOM) / 2.f}, {PANEL_W, CONTENT_TOP - CONTENT_BOTTOM});
        auto scroll = this->addScroll(page, TabKeybinds, {ROW_X, CONTENT_BOTTOM + PANEL_PAD}, {ROW_W, CONTENT_TOP - CONTENT_BOTTOM - PANEL_PAD * 2.f});

        scroll->m_contentLayer->addChild(
            this->makeKeybindRow("Open Menu", "key-open-menu", KEY_T));
        scroll->m_contentLayer->addChild(this->makeKeybindRow("Record", "key-record", KEY_F1));
        scroll->m_contentLayer->addChild(this->makeKeybindRow("Replay", "key-replay", KEY_F2));
        scroll->m_contentLayer->addChild(
            this->makeKeybindRow("Speedhack", "key-speedhack", KEY_Shift));
        scroll->m_contentLayer->addChild(
            this->makeKeybindRow("Frame Step", "key-frame-step", KEY_F3));
        scroll->m_contentLayer->addChild(
            this->makeKeybindRow("Toggle Stepper", "key-frame-stepper", KEY_F4));

        scroll->m_contentLayer->updateLayout();
        scroll->scrollToTop();
    }

    // about page
    {
        auto page = this->makePage(TabAbout).node;
        this->addPageTitle(page, "About", nullptr);

        this->addPanel(page, {PANEL_X, (CONTENT_TOP + CONTENT_BOTTOM) / 2.f}, {PANEL_W, CONTENT_TOP - CONTENT_BOTTOM});
        auto scroll = this->addScroll(page, TabAbout, {ROW_X, CONTENT_BOTTOM + PANEL_PAD}, {ROW_W, CONTENT_TOP - CONTENT_BOTTOM - PANEL_PAD * 2.f});

        scroll->m_contentLayer->addChild(this->makeSectionRow(Mod::get()->getName().data()));

        scroll->m_contentLayer->addChild(
            makeTextRow("ToastyReplay-Lite is a lightweight macro bot for Geometry Dash, with a "
                        "goal of making and playing macros simple.",
                        .6f));

        scroll->m_contentLayer->addChild(this->makeSectionRow("Developer"));

        auto devRow = CCNode::create();
        devRow->setContentSize({ROW_W, 22.f});
        auto dev = CCLabelBMFont::create(modDevelopers().c_str(), "goldFont.fnt");
        dev->setPosition({ROW_W / 2.f, 11.f});
        dev->limitLabelWidth(ROW_W - 20.f, .5f, .1f);
        devRow->addChild(dev);
        scroll->m_contentLayer->addChild(devRow);

        scroll->m_contentLayer->addChild(this->makeSectionRow("Version"));

        auto verRow = CCNode::create();
        verRow->setContentSize({ROW_W, 22.f});
        auto ver = CCLabelBMFont::create(modVersion().c_str(), "bigFont.fnt");
        ver->setScale(.4f);
        ver->setPosition({ROW_W / 2.f, 11.f});
        verRow->addChild(ver);
        scroll->m_contentLayer->addChild(verRow);

        scroll->m_contentLayer->addChild(this->makeSectionRow("Credits"));

        scroll->m_contentLayer->addChild(
            makeTextRow("HUGE THANKS to Peony, and developers of Silicate, and to Chag (owner of "
                        "TCBot) for having amazing reference points, and understanding.\n\n"
                        "Inspired by xdBot and zBot, go check them out!\n\n"
                        "Ko-fi icon by dank_meme (Globed).",
                        .55f));

        scroll->m_contentLayer->updateLayout();
        scroll->scrollToTop();
    }

    this->updateTabs();
    this->updatePages();
    this->schedule(schedule_selector(ToastyMenu::checkMacroDirectory), 1.f);

    s_instance = this;
    return true;
}

void ToastyMenu::addHeader() {
    float titleX = MARGIN;
    if (auto icon = CCSprite::create("MenuIcon.png"_spr)) {
        icon->setScale(HEADER_ICON / std::max(icon->getContentWidth(), icon->getContentHeight()));
        icon->setAnchorPoint({0.f, .5f});
        icon->setPosition({MARGIN, HEADER_Y});
        icon->setID("header-icon");
        m_mainLayer->addChild(icon);
        titleX = MARGIN + HEADER_ICON + GAP;
    }

    auto title = CCLabelBMFont::create("ToastyReplay Lite", "goldFont.fnt");
    title->setAnchorPoint({0.f, .5f});
    title->setPosition({titleX, HEADER_Y});
    title->limitLabelWidth(165.f, .5f, .1f);
    m_mainLayer->addChild(title);

    auto version = CCLabelBMFont::create(modVersion().c_str(), "bigFont.fnt");
    version->setAnchorPoint({0.f, .5f});
    version->setScale(.3f);
    version->setPosition({title->getPositionX() + title->getScaledContentWidth() + 12.f, HEADER_Y});
    m_mainLayer->addChild(version);

    auto divider = makeBG({POPUP_W - MARGIN * 2.f, 2.f}, {0, 0, 0}, 90, false);
    divider->setPosition({POPUP_W / 2.f, DIVIDER_Y});
    m_mainLayer->addChild(divider);
}

void ToastyMenu::addSidebar() {
    float sidebarHeight = CONTENT_TOP - COLUMN_BOTTOM;
    auto sidebar = makeBG({SIDE_W, sidebarHeight}, PANEL_COLOR, 220, false);
    sidebar->setPosition({SIDE_X, COLUMN_BOTTOM + sidebarHeight / 2.f});
    m_mainLayer->addChild(sidebar);

    auto menu = CCMenu::create();
    menu->setPosition({0.f, 0.f});
    menu->setContentSize(m_size);
    menu->setID("tab-menu");
    m_mainLayer->addChild(menu, 10);

    const char* tabNames[TabCount] = {"Main", "Macros", "Settings", "Keybinds", "About"};
    float tabTop = CONTENT_TOP - SIDE_PAD - TAB_H / 2.f;
    for (auto [i, name] : asp::iter::enumerate(tabNames)) {
        auto node = CCNode::create();
        node->setContentSize({TAB_W, TAB_H});

        auto bg = makeBG(node->getContentSize(), {0, 0, 0}, 70, true);
        bg->setPosition({TAB_W / 2.f, TAB_H / 2.f});
        node->addChild(bg);
        m_tabBgs[i] = bg;

        auto label = CCLabelBMFont::create(name, "bigFont.fnt");
        label->setAnchorPoint({0.f, .5f});
        label->setPosition({ROW_PAD, TAB_H / 2.f});
        label->limitLabelWidth(TAB_W - ROW_PAD * 2.f, .4f, .1f);
        node->addChild(label);

        auto item = CCMenuItemSpriteExtra::create(node, this, menu_selector(ToastyMenu::onTab));
        item->setTag(static_cast<int>(i));
        item->setPosition({SIDE_X, tabTop - static_cast<float>(i) * (TAB_H + TAB_GAP)});
        menu->addChild(item);
    }

    auto social = CCMenu::create();
    social->setID("social-menu");
    social->setAnchorPoint({.5f, .5f});
    social->setContentSize({SOCIAL_ICON * 3.f + CTRL_GAP * 2.f, SOCIAL_ICON});
    social->setPosition({SIDE_X, SOCIAL_Y});
    social->setLayout(
        RowLayout::create()->setGap(CTRL_GAP)->setAxisAlignment(AxisAlignment::Center));
    m_mainLayer->addChild(social, 10);

    this->addSocialButton(social,
                          CCSprite::create("KofiIcon.png"_spr),
                          "kofi-button",
                          "https://ko-fi.com/toastexgd");
    this->addSocialButton(social,
                          CCSprite::createWithSpriteFrameName("gj_ytIcon_001.png"),
                          "youtube-button",
                          "https://www.youtube.com/@Toastex_");
    this->addSocialButton(social,
                          CCSprite::createWithSpriteFrameName("geode.loader/github.png"),
                          "github-button",
                          "https://github.com/ToastexGD/ToastyReplay-Lite");
    social->updateLayout();
}

void ToastyMenu::addSocialButton(CCMenu* menu, CCSprite* icon, ZStringView id, ZStringView url) {
    if (!icon) {
        return;
    }
    icon->setScale(SOCIAL_ICON / std::max(icon->getContentWidth(), icon->getContentHeight()));

    auto item = CCMenuItemSpriteExtra::create(icon, this, menu_selector(ToastyMenu::onSocialLink));
    item->setID(id);
    item->setUserObject(CCString::create(url.c_str()));
    menu->addChild(item);
}

ToastyMenu::Group ToastyMenu::makePage(int tab) {
    Group page;
    page.node = CCNode::create();
    page.node->setContentSize(m_size);
    m_mainLayer->addChild(page.node);
    m_pages[tab] = page.node;

    page.menu = CCMenu::create();
    page.menu->setPosition({0.f, 0.f});
    page.menu->setContentSize(m_size);
    page.node->addChild(page.menu);
    return page;
}

void ToastyMenu::addPageTitle(CCNode* page, ZStringView title, ZStringView hint) {
    auto label = CCLabelBMFont::create(title.c_str(), "goldFont.fnt");
    label->setAnchorPoint({0.f, .5f});
    label->setPosition({PANEL_LEFT, TITLE_Y});
    label->limitLabelWidth(160.f, .5f, .1f);
    page->addChild(label);

    if (hint.empty())
        return;

    auto hintLabel = CCLabelBMFont::create(hint.c_str(), "chatFont.fnt");
    hintLabel->setAnchorPoint({1.f, .5f});
    hintLabel->setOpacity(120);
    hintLabel->setPosition({POPUP_W - MARGIN, TITLE_Y});
    hintLabel->limitLabelWidth(130.f, .4f, .1f);
    page->addChild(hintLabel);
}

geode::NineSlice* ToastyMenu::addPanel(CCNode* page, CCPoint center, CCSize size) {
    auto panel = makeBG(size, PANEL_COLOR, 220, false);
    panel->setPosition(center);
    page->addChild(panel);
    return panel;
}

ScrollLayer* ToastyMenu::addScroll(CCNode* page, int tab, CCPoint pos, CCSize size) {
    auto scroll = ScrollLayer::create(size);
    scroll->setPosition(pos);
    scroll->m_contentLayer->setLayout(ColumnLayout::create()
                                          ->setAxisReverse(true)
                                          ->setAxisAlignment(AxisAlignment::End)
                                          ->setAutoGrowAxis(size.height)
                                          ->setGap(4.f));
    page->addChild(scroll);
    m_pageTouchNodes[tab].push_back(scroll);

    auto bar = Scrollbar::create(scroll);
    bar->setPosition({SCROLLBAR_X, pos.y + size.height / 2.f});
    page->addChild(bar);
    m_pageTouchNodes[tab].push_back(bar);
    return scroll;
}

ToastyMenu::Group
ToastyMenu::makeRow(ZStringView title, float height, float titleWidth, bool enabled) {
    Group row;
    row.node = CCNode::create();
    row.node->setContentSize({ROW_W, height});

    auto bg = makeBG(row.node->getContentSize(), {0, 0, 0}, 45, true);
    bg->setPosition({ROW_W / 2.f, height / 2.f});
    row.node->addChild(bg);

    auto label = CCLabelBMFont::create(title.c_str(), "bigFont.fnt");
    label->setAnchorPoint({0.f, .5f});
    label->setPosition({10.f, height / 2.f});
    label->limitLabelWidth(titleWidth, .45f, .1f);
    if (!enabled) {
        label->setColor({145, 145, 145});
    }
    row.node->addChild(label);

    row.menu = CCMenu::create();
    row.menu->setPosition({0.f, 0.f});
    row.menu->setContentSize(row.node->getContentSize());
    row.node->addChild(row.menu);
    return row;
}

CCNode* ToastyMenu::makeToggleRow(ZStringView id, ZStringView title, bool on, bool enabled) {
    auto row = this->makeRow(title, ROW_H, 200.f, enabled);
    row.node->setID(id);

    auto toggle = CCMenuItemToggler::createWithStandardSprites(
        this, menu_selector(ToastyMenu::onToggleOption), .6f);
    placeCenter(toggle, CONTROL_CENTER, ROW_H / 2.f);
    toggle->toggle(on);
    toggle->setID(fmt::format("{}-toggle", id));
    toggle->setEnabled(enabled);
    if (!enabled) {
        toggle->setOpacity(130);
    }
    row.menu->addChild(toggle);
    return row.node;
}

CCNode* ToastyMenu::makeSeedRow() {
    auto row = this->makeRow("Seed", 32.f, 100.f);
    row.node->setID("set-seed");

    if (auto infoSpr = CCSprite::createWithSpriteFrameName("GJ_infoIcon_001.png")) {
        infoSpr->setScale(.5f);
        auto info =
            CCMenuItemSpriteExtra::create(infoSpr, this, menu_selector(ToastyMenu::onSeedInfo));
        info->setPosition({111.f, 16.f});
        row.menu->addChild(info);
    }

    if (auto leftSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png")) {
        leftSpr->setScale(.38f);
        auto left =
            CCMenuItemSpriteExtra::create(leftSpr, this, menu_selector(ToastyMenu::onSeedAdjust));
        left->setTag(-1);
        left->setPosition({137.f, 16.f});
        row.menu->addChild(left);
    }

    m_seedInput = TextInput::create(118.f, "Seed");
    m_seedInput->setCommonFilter(CommonFilter::Uint);
    m_seedInput->setMaxCharCount(20);
    m_seedInput->setScale(.55f);
    m_seedInput->setPosition({194.f, 16.f});
    m_seedInput->setString(fmt::format("{}", toasty::seed::value()));
    m_seedInput->setCallback([](std::string const& value) {
        auto result = utils::numFromString<uint64_t>(value);
        if (result) {
            toasty::seed::setValue(result.unwrapOr(1));
        }
    });
    row.node->addChild(m_seedInput);

    if (auto rightSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png")) {
        rightSpr->setScale(.38f);
        rightSpr->setFlipX(true);
        auto right =
            CCMenuItemSpriteExtra::create(rightSpr, this, menu_selector(ToastyMenu::onSeedAdjust));
        right->setTag(1);
        right->setPosition({250.f, 16.f});
        row.menu->addChild(right);
    }

    auto toggle = CCMenuItemToggler::createWithStandardSprites(
        this, menu_selector(ToastyMenu::onToggleOption), .6f);
    placeCenter(toggle, CONTROL_CENTER, 16.f);
    toggle->toggle(toasty::seed::enabled());
    toggle->setID("set-seed-toggle");
    row.menu->addChild(toggle);

    return row.node;
}

CCNode* ToastyMenu::makeSpeedhackRow() {
    auto row = this->makeRow("Speedhack", 32.f, 100.f);
    row.node->setID("speedhack");

    if (auto gearSpr = CCSprite::createWithSpriteFrameName("GJ_optionsBtn_001.png")) {
        gearSpr->setScale(.45f);
        auto gear = CCMenuItemSpriteExtra::create(
            gearSpr, this, menu_selector(ToastyMenu::onSpeedhackOptions));
        gear->setID("options");
        gear->setPosition({111.f, 16.f});
        row.menu->addChild(gear);
    }

    if (auto leftSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png")) {
        leftSpr->setScale(.38f);
        auto left = CCMenuItemSpriteExtra::create(
            leftSpr, this, menu_selector(ToastyMenu::onSpeedhackAdjust));
        left->setTag(-1);
        left->setPosition({137.f, 16.f});
        row.menu->addChild(left);
    }

    m_speedInput = TextInput::create(118.f, "Speed");
    m_speedInput->setCommonFilter(CommonFilter::Float);
    m_speedInput->setMaxCharCount(8);
    m_speedInput->setScale(.55f);
    m_speedInput->setPosition({194.f, 16.f});
    m_speedInput->setString(speedText(toasty::speedhack::rate()));
    m_speedInput->setCallback([](std::string const& value) {
        auto result = utils::numFromString<double>(value);
        if (!result) {
            return;
        }
        auto parsed = result.unwrapOr(toasty::speedhack::Default);
        if (parsed >= toasty::speedhack::Minimum && parsed <= toasty::speedhack::Maximum) {
            toasty::speedhack::setRate(parsed);
        }
    });
    row.node->addChild(m_speedInput);

    if (auto rightSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png")) {
        rightSpr->setScale(.38f);
        rightSpr->setFlipX(true);
        auto right = CCMenuItemSpriteExtra::create(
            rightSpr, this, menu_selector(ToastyMenu::onSpeedhackAdjust));
        right->setTag(1);
        right->setPosition({250.f, 16.f});
        row.menu->addChild(right);
    }

    m_speedToggle = CCMenuItemToggler::createWithStandardSprites(
        this, menu_selector(ToastyMenu::onSpeedhackToggle), .6f);
    placeCenter(m_speedToggle, CONTROL_CENTER, 16.f);
    m_speedToggle->toggle(toasty::speedhack::enabled());
    row.menu->addChild(m_speedToggle);

    return row.node;
}

CCNode* ToastyMenu::makeTpsRow() {
    auto row = this->makeRow("TPS Bypass", 32.f, 92.f);
    row.node->setID("tps-bypass");

    if (auto infoSpr = CCSprite::createWithSpriteFrameName("GJ_infoIcon_001.png")) {
        infoSpr->setScale(.5f);
        auto info =
            CCMenuItemSpriteExtra::create(infoSpr, this, menu_selector(ToastyMenu::onTpsInfo));
        info->setPosition({111.f, 16.f});
        row.menu->addChild(info);
    }

    if (auto leftSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png")) {
        leftSpr->setScale(.38f);
        auto left =
            CCMenuItemSpriteExtra::create(leftSpr, this, menu_selector(ToastyMenu::onTpsAdjust));
        left->setTag(-1);
        left->setPosition({137.f, 16.f});
        row.menu->addChild(left);
    }

    m_tpsInput = TextInput::create(118.f, "TPS");
    m_tpsInput->setCommonFilter(CommonFilter::Uint);
    m_tpsInput->setMaxCharCount(7);
    m_tpsInput->setScale(.55f);
    m_tpsInput->setPosition({194.f, 16.f});
    m_tpsInput->setString(fmt::format("{}", toasty::tps::rate()));
    m_tpsInput->setCallback([this](std::string const& value) {
        auto result = utils::numFromString<int64_t>(value);
        if (!result) {
            return;
        }
        auto parsed = result.unwrapOr(toasty::tps::Vanilla);
        if (parsed >= toasty::tps::Minimum && parsed <= toasty::tps::Maximum) {
            toasty::tps::setRate(parsed);
        }
    });
    row.node->addChild(m_tpsInput);

    if (auto rightSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png")) {
        rightSpr->setScale(.38f);
        rightSpr->setFlipX(true);
        auto right =
            CCMenuItemSpriteExtra::create(rightSpr, this, menu_selector(ToastyMenu::onTpsAdjust));
        right->setTag(1);
        right->setPosition({250.f, 16.f});
        row.menu->addChild(right);
    }

    m_tpsToggle = CCMenuItemToggler::createWithStandardSprites(
        this, menu_selector(ToastyMenu::onTpsToggle), .6f);
    placeCenter(m_tpsToggle, CONTROL_CENTER, 16.f);
    m_tpsToggle->toggle(toasty::tps::enabled());
    row.menu->addChild(m_tpsToggle);

    if (!toasty::tps::available()) {
        m_tpsInput->setString("Unavailable");
        m_tpsInput->setEnabled(false);
        m_tpsToggle->setEnabled(false);
    }

    return row.node;
}

CCNode* ToastyMenu::makeNoclipRow() {
    auto row = this->makeRow("Noclip", ROW_H, 140.f);
    row.node->setID("noclip");

    auto toggle = CCMenuItemToggler::createWithStandardSprites(
        this, menu_selector(ToastyMenu::onToggleOption), .6f);
    auto next = placeCenter(toggle, CONTROL_CENTER, ROW_H / 2.f);
    toggle->toggle(Mod::get()->getSavedValue<bool>("noclip", false));
    toggle->setID("noclip-toggle");
    row.menu->addChild(toggle);

    if (auto gearSpr = CCSprite::createWithSpriteFrameName("GJ_optionsBtn_001.png")) {
        gearSpr->setScale(.45f);
        auto gear = CCMenuItemSpriteExtra::create(
            gearSpr, this, menu_selector(ToastyMenu::onNoclipOptions));
        gear->setID("options");
        placeRight(gear, next, ROW_H / 2.f);
        row.menu->addChild(gear);
    }

    return row.node;
}

CCNode* ToastyMenu::makeFrameStepperRow() {
    auto row = this->makeRow("Frame Stepper", ROW_H, 140.f);
    row.node->setID("frame-stepper");

    auto toggle = CCMenuItemToggler::createWithStandardSprites(
        this, menu_selector(ToastyMenu::onFrameStepperToggle), .6f);
    auto next = placeCenter(toggle, CONTROL_CENTER, ROW_H / 2.f);
    toggle->toggle(toasty::stepper::enabled());
    toggle->setID("frame-stepper-toggle");
    row.menu->addChild(toggle);

    if (auto gearSpr = CCSprite::createWithSpriteFrameName("GJ_optionsBtn_001.png")) {
        gearSpr->setScale(.45f);
        auto gear = CCMenuItemSpriteExtra::create(
            gearSpr, this, menu_selector(ToastyMenu::onFrameStepperOptions));
        gear->setID("options");
        placeRight(gear, next, ROW_H / 2.f);
        row.menu->addChild(gear);
    }

    return row.node;
}

CCNode* ToastyMenu::makeSectionRow(ZStringView title) {
    auto row = CCNode::create();
    row->setContentSize({ROW_W, 20.f});

    auto bg = makeBG(row->getContentSize(), {0, 0, 0}, 80, true);
    bg->setPosition({ROW_W / 2.f, 10.f});
    row->addChild(bg);

    auto label = CCLabelBMFont::create(title.c_str(), "goldFont.fnt");
    label->setPosition({ROW_W / 2.f, 10.f});
    label->limitLabelWidth(ROW_W - 20.f, .35f, .1f);
    row->addChild(label);
    return row;
}

CCNode* ToastyMenu::makeMacroRow(std::string const& fileName, int index) {
    auto row = CCNode::create();
    row->setContentSize({ROW_W, MACRO_ROW_H});
    float middle = MACRO_ROW_H / 2.f;

    auto bg = makeBG(row->getContentSize(), {0, 0, 0}, 45, true);
    bg->setPosition({ROW_W / 2.f, middle});
    row->addChild(bg);
    m_macroRowBgs.push_back(bg);

    auto menu = CCMenu::create();
    menu->setPosition({0.f, 0.f});
    menu->setContentSize(row->getContentSize());
    row->addChild(menu);

    float next = ROW_CONTROL_RIGHT;

    if (auto deleteSpr = CCSprite::createWithSpriteFrameName("GJ_deleteBtn_001.png")) {
        deleteSpr->setScale(.5f);
        auto deleteBtn = CCMenuItemSpriteExtra::create(
            deleteSpr, this, menu_selector(ToastyMenu::onDeleteMacro));
        deleteBtn->setTag(index);
        deleteBtn->setID("delete");
        setMacroName(deleteBtn, fileName);
        next = placeRight(deleteBtn, next, middle);
        menu->addChild(deleteBtn);
    }

    if (auto renameSpr = CCSprite::createWithSpriteFrameName("GJ_optionsBtn_001.png")) {
        renameSpr->setScale(.45f);
        auto renameBtn = CCMenuItemSpriteExtra::create(
            renameSpr, this, menu_selector(ToastyMenu::onRenameMacro));
        renameBtn->setTag(index);
        renameBtn->setID("rename");
        setMacroName(renameBtn, fileName);
        next = placeRight(renameBtn, next, middle);
        menu->addChild(renameBtn);
    }

    float nameWidth = next - ROW_PAD;

    auto displayName = toasty::replay::ttrl::displayName(fileName);
    auto label = CCLabelBMFont::create(displayName.c_str(), "chatFont.fnt");
    label->setAnchorPoint({0.f, .5f});
    label->setPosition({ROW_PAD, middle});
    label->limitLabelWidth(nameWidth, .65f, .1f);
    row->addChild(label);

    if (auto hitSpr = CCSprite::create("square02b_001.png")) {
        hitSpr->setOpacity(0);
        hitSpr->setContentSize({nameWidth + ROW_PAD, MACRO_ROW_H});
        auto hitBtn =
            CCMenuItemSpriteExtra::create(hitSpr, this, menu_selector(ToastyMenu::onSelectMacro));
        hitBtn->setTag(index);
        hitBtn->setPosition({(nameWidth + ROW_PAD) / 2.f, middle});
        hitBtn->setID("select");
        setMacroName(hitBtn, fileName);
        menu->addChild(hitBtn);
    }

    return row;
}

CCNode* ToastyMenu::makeKeybindRow(ZStringView title, ZStringView saveId, enumKeyCodes def) {
    auto row = this->makeRow(title, 32.f, 190.f);
    row.node->setID(saveId);

    auto saved =
        static_cast<enumKeyCodes>(Mod::get()->getSavedValue<int>(saveId, static_cast<int>(def)));
    auto spr =
        ButtonSprite::create(keyName(saved).c_str(), "goldFont.fnt", "GJ_button_04.png", .8f);
    spr->setScale(.55f);
    auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(ToastyMenu::onBindKey));
    btn->setUserObject(CCString::create(saveId.c_str()));
    btn->setID("bind");
    placeRight(btn, ROW_CONTROL_RIGHT, 16.f);
    row.menu->addChild(btn);
    return row.node;
}

void ToastyMenu::show() {
    Popup::show();

    float scale = Mod::get()->getSavedValue<float>("menu-scale", 1.f);

    m_mainLayer->stopAllActions();
    m_mainLayer->setScale(scale);

    float savedX = Mod::get()->getSavedValue<float>("menu-pos-x", -10000.f);
    float savedY = Mod::get()->getSavedValue<float>("menu-pos-y", -10000.f);
    if (savedX > -9999.f && savedY > -9999.f) {
        m_mainLayer->setPosition({savedX, savedY});
    }
    this->clampMainLayer();

    m_mainLayer->setScale(0.f);
    m_mainLayer->runAction(CCEaseElasticOut::create(CCScaleTo::create(.5f, scale), .6f));

    this->updateModes();

    toasty::ui::refreshWatermark();

    // gd hides the cursor in levels
    PlatformToolbox::showCursor();
}

void ToastyMenu::clampMainLayer() {
    auto win = CCDirector::sharedDirector()->getWinSize();
    auto bb = m_mainLayer->boundingBox();
    float dx = 0.f, dy = 0.f;
    if (bb.getMinX() < 0.f)
        dx = -bb.getMinX();
    else if (bb.getMaxX() > win.width)
        dx = win.width - bb.getMaxX();
    if (bb.getMinY() < 0.f)
        dy = -bb.getMinY();
    else if (bb.getMaxY() > win.height)
        dy = win.height - bb.getMaxY();
    m_mainLayer->setPosition(m_mainLayer->getPosition() + CCPoint{dx, dy});
}

bool ToastyMenu::ccTouchBegan(CCTouch* touch, CCEvent* event) {
    FLAlertLayer::ccTouchBegan(touch, event);

    CCRect grip = {0.f, DIVIDER_Y, m_size.width, m_size.height - DIVIDER_Y};
    if (grip.containsPoint(m_mainLayer->convertTouchToNodeSpace(touch))) {
        m_dragging = true;
        m_dragOffset = m_mainLayer->getPosition() - this->convertTouchToNodeSpace(touch);
    }
    return true;
}

void ToastyMenu::ccTouchMoved(CCTouch* touch, CCEvent* event) {
    if (m_dragging) {
        m_mainLayer->setPosition(this->convertTouchToNodeSpace(touch) + m_dragOffset);
        this->clampMainLayer();
    } else {
        FLAlertLayer::ccTouchMoved(touch, event);
    }
}

void ToastyMenu::ccTouchEnded(CCTouch* touch, CCEvent* event) {
    if (m_dragging) {
        auto pos = m_mainLayer->getPosition();
        Mod::get()->setSavedValue<float>("menu-pos-x", pos.x);
        Mod::get()->setSavedValue<float>("menu-pos-y", pos.y);
    }
    m_dragging = false;
    FLAlertLayer::ccTouchEnded(touch, event);
}

bool ToastyMenu::handleKey(enumKeyCodes key, bool down, bool repeat) {
    if (CCIMEDispatcher::sharedDispatcher()->hasDelegate()) {
        return false;
    }

    auto stepKey = Mod::get()->getSavedValue<int>("key-frame-step", static_cast<int>(KEY_F3));
    if (!s_captureBtn && static_cast<int>(key) == stepKey) {
        if (down && !repeat) {
            toasty::stepper::stepOnce();
            toasty::stepper::setKeyHeld(true);
        } else if (!down) {
            toasty::stepper::setKeyHeld(false);
        }
        return true;
    }

    if (!down || repeat) {
        return false;
    }

    if (s_captureBtn) {
        if (key == KEY_Escape) {
            s_captureBtn->setString(s_capturePrev.c_str());
        } else {
            Mod::get()->setSavedValue<int>(s_captureId, static_cast<int>(key));
            s_captureBtn->setString(keyName(key).c_str());
        }
        s_captureBtn = nullptr;
        return true;
    }
    auto openKey = Mod::get()->getSavedValue<int>("key-open-menu", static_cast<int>(KEY_T));
    if (static_cast<int>(key) == openKey) {
        if (s_instance)
            s_instance->onClose(nullptr);
        else
            ToastyMenu::create()->show();
        return true;
    }

    auto recordKey = Mod::get()->getSavedValue<int>("key-record", static_cast<int>(KEY_F1));
    if (static_cast<int>(key) == recordKey) {
        if (toasty::engine::recording()) {
            toasty::engine::stopRecording(true);
        } else {
            startRecordingFromMenu();
        }
        return true;
    }

    auto replayKey = Mod::get()->getSavedValue<int>("key-replay", static_cast<int>(KEY_F2));
    if (static_cast<int>(key) == replayKey) {
        toasty::engine::togglePlayback();
        return true;
    }

    auto stepperKey =
        Mod::get()->getSavedValue<int>("key-frame-stepper", static_cast<int>(KEY_F4));
    if (static_cast<int>(key) == stepperKey) {
        toasty::stepper::setEnabled(!toasty::stepper::enabled());
        toasty::ui::refreshStepperButtons();
        return true;
    }

    auto speedKey = Mod::get()->getSavedValue<int>("key-speedhack", static_cast<int>(KEY_Shift));
    if (static_cast<int>(key) == speedKey) {
        toasty::speedhack::setEnabled(!toasty::speedhack::enabled());
        if (s_instance && s_instance->m_speedToggle) {
            s_instance->m_speedToggle->toggle(toasty::speedhack::enabled());
        }
        return true;
    }

    return false;
}

void ToastyMenu::updateModes() {
    auto mode = static_cast<int>(toasty::engine::mode());
    for (int i = 0; i < 3; i++) {
        bool on = i == mode;
        m_modeBgs[i]->setColor(on ? ccColor3B{255, 255, 255} : ccColor3B{95, 95, 95});
        m_modeBgs[i]->setOpacity(on ? 255 : 190);
        m_modeLabels[i]->setColor(on ? ccColor3B{255, 255, 255} : ccColor3B{195, 195, 195});
    }
}

void ToastyMenu::updateTabs() {
    for (int i = 0; i < TabCount; i++) {
        if (i == m_tab) {
            m_tabBgs[i]->setColor(m_accentColor);
            m_tabBgs[i]->setOpacity(200);
        } else {
            m_tabBgs[i]->setColor({0, 0, 0});
            m_tabBgs[i]->setOpacity(70);
        }
    }
}

void ToastyMenu::updatePages() {
    for (int i = 0; i < TabCount; i++) {
        bool visible = i == m_tab;
        m_pages[i]->setVisible(visible);
        setInteraction(m_pages[i], visible);
    }
}

void ToastyMenu::refreshMacroList(bool keepScroll) {
    if (!m_macroScroll) {
        return;
    }
    toasty::replay::ttrl::Storage storage(toasty::replay::ttrl::defaultReplayDirectory());
    auto files = storage.list();
    if (files.isErr()) {
        FLAlertLayer::create("Load Failed",
                             toasty::replay::ttrl::describe(files.unwrapErr()),
                             "OK")
            ->show();
        return;
    }
    m_macroNames = std::move(files.unwrap());
    m_macroDirTime = macroDirectoryTime();
    auto selected = std::find(
        m_macroNames.begin(), m_macroNames.end(), toasty::engine::selectedReplay());
    m_selectedMacro = selected == m_macroNames.end()
                          ? -1
                          : static_cast<int>(std::distance(m_macroNames.begin(), selected));
    if (m_selectedMacro < 0) {
        toasty::engine::setSelectedReplay({});
    }
    m_macroRowBgs.clear();
    auto content = m_macroScroll->m_contentLayer;
    float viewHeight = m_macroScroll->getContentHeight();
    float scrolled = keepScroll
                         ? content->getPositionY() - viewHeight + content->getContentHeight()
                         : 0.f;

    content->removeAllChildrenWithCleanup(true);
    for (size_t index = 0; index < m_macroNames.size(); ++index) {
        content->addChild(this->makeMacroRow(m_macroNames[index], static_cast<int>(index)));
    }
    content->updateLayout();

    if (keepScroll) {
        float limit = std::max(0.f, content->getContentHeight() - viewHeight);
        content->setPositionY(viewHeight - content->getContentHeight() +
                              std::clamp(scrolled, 0.f, limit));
    } else {
        m_macroScroll->scrollToTop();
    }
    for (auto [index, bg] : asp::iter::enumerate(m_macroRowBgs)) {
        bg->setColor(static_cast<int>(index) == m_selectedMacro ? m_accentColor
                                                               : ccColor3B{0, 0, 0});
    }
    this->updateMacroInfo();
}

void ToastyMenu::checkMacroDirectory(float) {
    if (m_tab != TabMacros) {
        return;
    }
    if (macroDirectoryTime() == m_macroDirTime) {
        return;
    }
    this->refreshMacroList(true);
}

void ToastyMenu::updateMacroInfo() {
    if (!m_macroInfoLabel) {
        return;
    }

    auto name = toasty::engine::selectedReplay();
    if (name.empty()) {
        m_macroInfo = {};
        m_macroInfoName.clear();
        m_macroInfoLabel->setString("No macro selected");
        m_macroInfoLabel->setColor({150, 150, 150});
        m_macroInfoLabel->setPositionX(m_macroInfoRightWide);
        m_macroInfoLabel->limitLabelWidth(
            m_macroInfoRightWide - m_macroInfoLeft, FOOTER_TEXT_SCALE, .1f);
        if (m_macroInfoButton) {
            m_macroInfoButton->setVisible(false);
        }
        return;
    }

    if (name != m_macroInfoName) {
        m_macroInfoName = name;
        m_macroInfo = {};

        toasty::replay::ttrl::Storage storage(toasty::replay::ttrl::defaultReplayDirectory());
        auto loaded = storage.load(name);
        if (loaded.isOk()) {
            auto replay = std::move(loaded.unwrap());
            auto rate = replay.tps.normalized();

            m_macroInfo.loaded = true;
            m_macroInfo.inputs = replay.inputs.size();
            m_macroInfo.frameFixes = replay.frameFixes.size();
            m_macroInfo.tickCount = replay.tickCount;
            m_macroInfo.tps = rate ? static_cast<double>(rate->numerator) /
                                         static_cast<double>(rate->denominator)
                                   : 0.0;
            m_macroInfo.duration =
                m_macroInfo.tps > 0.0 ? static_cast<double>(replay.tickCount) / m_macroInfo.tps
                                      : 0.0;
            m_macroInfo.gameVersion = replay.gameVersion;
            m_macroInfo.levelId = replay.levelId;
            m_macroInfo.levelRevision = replay.levelRevision;
            m_macroInfo.platformer = replay.mode == toasty::replay::PlayMode::Platformer;
            m_macroInfo.seed = replay.seed;
            m_macroInfo.startPos = replay.startPos;
        }
    }

    bool showInfo = m_macroInfo.loaded && m_macroInfoButton != nullptr;
    float right = showInfo ? m_macroInfoRight : m_macroInfoRightWide;

    if (m_macroInfo.loaded) {
        m_macroInfoLabel->setString(fmt::format("{} actions", m_macroInfo.inputs).c_str());
        m_macroInfoLabel->setColor({255, 255, 255});
    } else {
        m_macroInfoLabel->setString("Unreadable macro");
        m_macroInfoLabel->setColor({255, 130, 130});
    }
    m_macroInfoLabel->setPositionX(right);
    m_macroInfoLabel->limitLabelWidth(right - m_macroInfoLeft, FOOTER_TEXT_SCALE, .1f);
    if (m_macroInfoButton) {
        m_macroInfoButton->setVisible(m_macroInfo.loaded);
    }
}

void ToastyMenu::onMacroInfo(CCObject*) {
    if (!m_macroInfo.loaded) {
        return;
    }

    auto level = m_macroInfo.levelId == 0
                     ? std::string("Local level")
                     : fmt::format("#{}", m_macroInfo.levelId);
    if (m_macroInfo.levelRevision != 0) {
        level += fmt::format(" (rev {})", m_macroInfo.levelRevision);
    }

    auto text = fmt::format("<cg>{}</c>\n\n"
                            "<cy>Level:</c> {}\n"
                            "<cy>Actions:</c> {}\n"
                            "<cy>Duration:</c> {}\n"
                            "<cy>Ticks:</c> {}\n"
                            "<cy>TPS:</c> {}\n"
                            "<cy>Mode:</c> {}\n"
                            "<cy>Game Version:</c> {}\n"
                            "<cy>Start:</c> {}\n"
                            "<cy>Seed:</c> {}\n"
                            "<cy>Frame Fixes:</c> {}",
                            toasty::replay::ttrl::displayName(m_macroInfoName),
                            level,
                            m_macroInfo.inputs,
                            durationText(m_macroInfo.duration),
                            m_macroInfo.tickCount,
                            speedText(m_macroInfo.tps),
                            m_macroInfo.platformer ? "Platformer" : "Classic",
                            gameVersionText(m_macroInfo.gameVersion),
                            m_macroInfo.startPos
                                ? fmt::format("Start position at x {:.0f}", *m_macroInfo.startPos)
                                : std::string("Level start"),
                            m_macroInfo.seed ? fmt::format("{}", *m_macroInfo.seed)
                                             : std::string("Not stored"),
                            m_macroInfo.frameFixes);

    FLAlertLayer::create(nullptr, "Macro Info", text, "OK", nullptr, 340.f, true, 180.f, .7f)
        ->show();
}

void ToastyMenu::onMode(CCObject* sender) {
    auto mode = static_cast<CCNode*>(sender)->getTag();
    if (mode == 0) {
        if (toasty::engine::recording()) {
            toasty::engine::stopRecording(true);
        }
        if (toasty::engine::playing()) {
            toasty::engine::stopPlayback();
        }
    } else if (mode == 1) {
        this->onClose(nullptr);
        queueInMainThread(startRecordingFromMenu);
        return;
    } else {
        if (toasty::engine::recording()) {
            toasty::engine::stopRecording(true);
        }
        if (toasty::engine::selectedReplay().empty()) {
            FLAlertLayer::create("Select Macro", "Pick a replay from the Macro List first", "OK")
                ->show();
        } else {
            auto name = toasty::engine::selectedReplay();
            this->onClose(nullptr);
            queueInMainThread([name = std::move(name)] { startReplayFromMenu(name); });
            return;
        }
    }
    this->updateModes();
}

void ToastyMenu::onTab(CCObject* sender) {
    m_tab = static_cast<CCNode*>(sender)->getTag();
    if (m_tab == TabMacros) {
        this->refreshMacroList();
    }
    this->updateTabs();
    this->updatePages();
}

void ToastyMenu::onToggleOption(CCObject* sender) {
    auto toggle = static_cast<CCMenuItemToggler*>(sender);
    auto row = static_cast<CCNode*>(toggle->getParent()->getParent());
    auto id = row->getID();
    if (!id.empty()) {
        auto next = !toggle->isToggled();
        Mod::get()->setSavedValue<bool>(id, next);
    }
}

void ToastyMenu::onSpeedhackToggle(CCObject* sender) {
    auto toggle = static_cast<CCMenuItemToggler*>(sender);
    toasty::speedhack::setEnabled(!toggle->isToggled());
}

void ToastyMenu::onSeedAdjust(CCObject* sender) {
    auto direction = static_cast<CCNode*>(sender)->getTag();
    auto current = toasty::seed::value();
    auto next = direction < 0 ? (current > 0 ? current - 1 : 0) : current + 1;
    toasty::seed::setValue(next);
    m_seedInput->setString(fmt::format("{}", next));
}

void ToastyMenu::onSpeedhackOptions(CCObject*) {
    if (auto popup = OptionsPopup::create(
            "Speedhack",
            {{"speedhack-audio",
              "Speedhack Audio",
              false,
              "Speeds up or slows down the level music to match the speedhack rate."}})) {
        popup->show();
    }
}

void ToastyMenu::onSpeedhackAdjust(CCObject* sender) {
    auto direction = static_cast<CCNode*>(sender)->getTag();
    auto next = std::clamp(toasty::speedhack::rate() + static_cast<double>(direction) * .1,
                           toasty::speedhack::Minimum,
                           toasty::speedhack::Maximum);
    next = std::round(next * 1000.0) / 1000.0;
    toasty::speedhack::setRate(next);
    m_speedInput->setString(speedText(next));
}

void ToastyMenu::onTpsToggle(CCObject* sender) {
    auto toggle = static_cast<CCMenuItemToggler*>(sender);
    auto next = !toggle->isToggled();
    if (!toasty::tps::setEnabled(next)) {
        toggle->toggle(false);
        this->onTpsInfo(nullptr);
    }
}

void ToastyMenu::onTpsAdjust(CCObject* sender) {
    auto direction = static_cast<CCNode*>(sender)->getTag();
    auto next = std::clamp<int64_t>(toasty::tps::rate() + static_cast<int64_t>(direction) * 60,
                                    toasty::tps::Minimum,
                                    toasty::tps::Maximum);
    toasty::tps::setRate(next);
    m_tpsInput->setString(fmt::format("{}", next));
}

void ToastyMenu::onFrameStepperToggle(CCObject* sender) {
    auto toggle = static_cast<CCMenuItemToggler*>(sender);
    toasty::stepper::setEnabled(!toggle->isToggled());
    toasty::ui::refreshStepperButtons();
}

void ToastyMenu::onFrameStepperOptions(CCObject*) {
    if (auto popup = OptionsPopup::create(
            "Frame Stepper",
            {{"stepper-buttons", "On Screen Buttons", toasty::ui::stepperButtonsDefault(), ""},
             {"stepper-override-tps",
              "Override TPS Limit",
              false,
              "Always forces frame stepper to step on 240tps intervals, regardless of TPS."}})) {
        popup->show();
    }
}

void ToastyMenu::onNoclipOptions(CCObject*) {
    if (auto popup = OptionsPopup::create(
            "Noclip", {{"noclip-p1", "Player 1", true}, {"noclip-p2", "Player 2", true}})) {
        popup->show();
    }
}

void ToastyMenu::onTpsInfo(CCObject* sender) {
    if (!toasty::tps::available()) {
        FLAlertLayer::create("TPS Bypass Unavailable", toasty::tps::unavailableReason(), "OK")
            ->show();
        return;
    }

    FLAlertLayer::create(
        "TPS Bypass",
        "Runs level physics at the selected TPS without changing game speed. Changes apply on the "
        "next physics update. Higher values use more CPU, and manual input precision still depends "
        "on your input and display rate. Keep every other physics or TPS bypass disabled.",
        "OK")
        ->show();
}

void ToastyMenu::onSeedInfo(CCObject*) {
    FLAlertLayer::create(
        "Seed",
        "Uses the selected Geometry Dash random seed when recording and saves it in the replay. "
        "Playback restores the seed stored in the replay.",
        "OK")
        ->show();
}

void ToastyMenu::onSelectMacro(CCObject* sender) {
    auto index = static_cast<CCNode*>(sender)->getTag();
    auto name = macroNameFromSender(sender);
    if (name.empty() || index < 0 || index >= static_cast<int>(m_macroRowBgs.size())) {
        return;
    }
    toasty::engine::setSelectedReplay(std::move(name));
    m_selectedMacro = index;
    for (auto [i, bg] : asp::iter::enumerate(m_macroRowBgs)) {
        bg->setColor(i == index ? m_accentColor : ccColor3B{0, 0, 0});
    }
    this->updateMacroInfo();
}

void ToastyMenu::finishAddMacroFile(std::optional<std::filesystem::path> picked) {
    if (!picked) {
        return;
    }

    toasty::replay::ttrl::Storage storage(toasty::replay::ttrl::defaultReplayDirectory());
    auto imported = storage.importFile(*picked);
    if (imported.isErr()) {
        FLAlertLayer::create("Import Failed",
                             toasty::replay::ttrl::describe(imported.unwrapErr()),
                             "OK")
            ->show();
        return;
    }

    if (s_instance) {
        s_instance->refreshMacroList();
    }
    toasty::notifications::show(
        fmt::format("Added {}", toasty::replay::ttrl::displayName(imported.unwrap())),
        NotificationIcon::Success);
}

void ToastyMenu::onAddMacroFile(CCObject*) {
    utils::file::FilePickOptions options;
    options.filters.push_back({"ToastyReplay Macro", {"*.ttrl"}});

    async::spawn([options = std::move(options)]() -> arc::Future<void> {
        auto result = co_await utils::file::pick(utils::file::PickMode::OpenFile, options);
        queueInMainThread([result = std::move(result)]() mutable {
            if (result.isErr()) {
                toasty::notifications::show("Unable to open the file picker",
                                            NotificationIcon::Error);
                return;
            }
            ToastyMenu::finishAddMacroFile(std::move(result).unwrap());
        });
    });
}

void ToastyMenu::onOpenFolder(CCObject*) {
    geode::utils::file::openFolder(Mod::get()->getSaveDir());
}

void ToastyMenu::onSocialLink(CCObject* sender) {
    auto node = typeinfo_cast<CCNode*>(sender);
    auto url = node ? typeinfo_cast<CCString*>(node->getUserObject()) : nullptr;
    if (!url) {
        return;
    }
    geode::utils::web::openLinkInBrowser(url->getCString());
}

void ToastyMenu::onAccentColor(CCObject*) {
    if (auto popup = ColorPickPopup::create(m_accentColor)) {
        WeakRef<ToastyMenu> weak(this);
        popup->setCallback([weak](ccColor4B const& color) {
            if (auto menu = weak.lock()) {
                menu->setAccentColor(to3B(color));
            }
        });
        popup->show();
    }
}

void ToastyMenu::setAccentColor(ccColor3B color) {
    m_accentColor = color;
    Mod::get()->setSavedValue<ccColor3B>("accent-color", color);
    if (m_accentSwatch) {
        m_accentSwatch->setColor(color);
    }
    this->updateTabs();
    for (auto [index, bg] : asp::iter::enumerate(m_macroRowBgs)) {
        bg->setColor(static_cast<int>(index) == m_selectedMacro ? color : ccColor3B{0, 0, 0});
    }
}

void ToastyMenu::onScaleSlider(CCObject* sender) {
    float scale = .7f + m_scaleSlider->getValue() * .4f;
    Mod::get()->setSavedValue<float>("menu-scale", scale);
    m_mainLayer->setScale(scale);
    m_scalePct->setString(fmt::format("{}%", static_cast<int>(std::round(scale * 100.f))).c_str());
    this->clampMainLayer();
}

void ToastyMenu::onBindKey(CCObject* sender) {
    auto item = static_cast<CCMenuItemSpriteExtra*>(sender);
    auto spr = static_cast<ButtonSprite*>(item->getNormalImage());
    auto id = static_cast<CCString*>(item->getUserObject());
    if (!spr || !id)
        return;
    if (s_captureBtn)
        s_captureBtn->setString(s_capturePrev.c_str());
    s_captureBtn = spr;
    s_captureId = id->getCString();
    s_capturePrev = spr->m_label ? spr->m_label->getString() : "";
    spr->setString("...");
}

void ToastyMenu::onClose(CCObject* sender) {
    if (auto noclip =
            typeinfo_cast<CCMenuItemToggler*>(this->getChildByIDRecursive("noclip-toggle"))) {
        Mod::get()->setSavedValue("noclip", noclip->isToggled());
    }

    if (s_instance == this)
        s_instance = nullptr;
    s_captureBtn = nullptr;

    toasty::ui::refreshStepperButtons();
    toasty::ui::refreshWatermark();
    toasty::speedhack::syncAudio();

    // rehide cursor during gameplay
    if (PlayLayer::get()) {
        auto scene = CCDirector::sharedDirector()->getRunningScene();
        if (scene && !scene->getChildByType<PauseLayer>(0)) {
            PlatformToolbox::hideCursor();
        }
    }
    Popup::onClose(sender);
}

void ToastyMenu::onReplayMacro(CCObject*) {
    auto name = toasty::engine::selectedReplay();
    if (name.empty()) {
        FLAlertLayer::create("Select Macro", "Pick a replay from the Macro List first", "OK")
            ->show();
        return;
    }
    this->onClose(nullptr);
    queueInMainThread([name = std::move(name)] { startReplayFromMenu(name); });
}

void ToastyMenu::onRenameMacro(CCObject* sender) {
    auto name = macroNameFromSender(sender);
    if (name.empty()) {
        return;
    }
    if (auto popup = RenameMacroPopup::create(this, std::move(name))) {
        popup->show();
    }
}

void ToastyMenu::onDeleteMacro(CCObject* sender) {
    auto name = macroNameFromSender(sender);
    if (name.empty()) {
        return;
    }
    WeakRef<ToastyMenu> weak(this);
    createQuickPopup(
        "Delete Macro",
        fmt::format("Delete <cy>{}</c>?\nThis cannot be undone.",
                    toasty::replay::ttrl::displayName(name)),
        "Cancel",
        "Delete",
        [weak, name](auto, bool confirmed) {
            if (!confirmed) {
                return;
            }
            queueInMainThread([weak, name] {
                toasty::replay::ttrl::Storage storage(
                    toasty::replay::ttrl::defaultReplayDirectory());
                auto result = storage.remove(name);
                auto menu = weak.lock();
                if (result.isErr()) {
                    FLAlertLayer::create("Delete Failed",
                                         toasty::replay::ttrl::describe(result.unwrapErr()),
                                         "OK")
                        ->show();
                    return;
                }
                if (toasty::engine::selectedReplay() == name) {
                    toasty::engine::setSelectedReplay({});
                    if (toasty::engine::playing()) {
                        toasty::engine::stopPlayback();
                    }
                }
                if (menu) {
                    menu->refreshMacroList(true);
                }
            });
        });
}

OptionsPopup* OptionsPopup::create(std::string title, std::vector<Option> options) {
    auto popup = new OptionsPopup();
    if (popup->init(std::move(title), std::move(options))) {
        popup->autorelease();
        return popup;
    }
    delete popup;
    return nullptr;
}

bool OptionsPopup::init(std::string title, std::vector<Option> options) {
    auto height = 74.f + static_cast<float>(options.size()) * 30.f;
    if (!Popup::init(260.f, height)) {
        return false;
    }

    this->setTitle(title);
    moveCloseTopRight(m_closeBtn, m_mainLayer, m_size);

    auto menu = CCMenu::create();
    menu->setPosition({0.f, 0.f});
    menu->setContentSize(m_size);
    m_mainLayer->addChild(menu);

    auto y = height - 62.f;
    for (auto const& option : options) {
        auto label = CCLabelBMFont::create(option.title.c_str(), "bigFont.fnt");
        label->setAnchorPoint({0.f, .5f});
        label->setPosition({24.f, y});
        label->limitLabelWidth(150.f, .5f, .1f);
        m_mainLayer->addChild(label);

        if (!option.info.empty()) {
            if (auto infoSpr = CCSprite::createWithSpriteFrameName("GJ_infoIcon_001.png")) {
                infoSpr->setScale(.45f);
                auto info = CCMenuItemSpriteExtra::create(
                    infoSpr, this, menu_selector(OptionsPopup::onInfo));
                info->setPosition({190.f, y});
                info->setUserObject(CCString::create(option.info));
                menu->addChild(info);
            }
        }

        auto toggle = CCMenuItemToggler::createWithStandardSprites(
            this, menu_selector(OptionsPopup::onToggle), .6f);
        toggle->setPosition({222.f, y});
        toggle->toggle(Mod::get()->getSavedValue<bool>(option.id, option.defaultValue));
        toggle->setID(option.id);
        menu->addChild(toggle);

        y -= 30.f;
    }
    return true;
}

void OptionsPopup::onInfo(CCObject* sender) {
    auto node = typeinfo_cast<CCNode*>(sender);
    auto text = node ? typeinfo_cast<CCString*>(node->getUserObject()) : nullptr;
    if (!text) {
        return;
    }
    auto title = m_title ? m_title->getString() : "Info";
    FLAlertLayer::create(title, text->getCString(), "OK")->show();
}

void OptionsPopup::onToggle(CCObject* sender) {
    auto toggle = static_cast<CCMenuItemToggler*>(sender);
    auto id = toggle->getID();
    if (!id.empty()) {
        Mod::get()->setSavedValue<bool>(id, !toggle->isToggled());
    }
}

RenameMacroPopup* RenameMacroPopup::create(ToastyMenu* menu, std::string fileName) {
    auto popup = new RenameMacroPopup();
    if (popup->init(menu, std::move(fileName))) {
        popup->autorelease();
        return popup;
    }
    delete popup;
    return nullptr;
}

bool RenameMacroPopup::init(ToastyMenu* menu, std::string fileName) {
    if (!Popup::init(300.f, 160.f)) {
        return false;
    }

    m_menu = WeakRef(menu);
    m_fileName = std::move(fileName);
    this->setTitle("Rename Macro");
    moveCloseTopRight(m_closeBtn, m_mainLayer, m_size);

    m_input = TextInput::create(230.f, "Macro name");
    m_input->setCommonFilter(CommonFilter::Name);
    m_input->setMaxCharCount(toasty::replay::ttrl::MaximumReplayName);
    m_input->setString(toasty::replay::ttrl::displayName(m_fileName));
    m_input->setPosition({150.f, 88.f});
    m_input->setID("name-input");
    m_mainLayer->addChild(m_input);

    auto menuNode = CCMenu::create();
    menuNode->setPosition({0.f, 0.f});
    menuNode->setContentSize(m_size);
    m_mainLayer->addChild(menuNode);

    auto saveSprite = ButtonSprite::create("Save", "bigFont.fnt", "GJ_button_01.png", .8f);
    saveSprite->setScale(.7f);
    auto saveButton =
        CCMenuItemSpriteExtra::create(saveSprite, this, menu_selector(RenameMacroPopup::onSave));
    saveButton->setPosition({150.f, 40.f});
    saveButton->setID("save");
    menuNode->addChild(saveButton);
    return true;
}

void RenameMacroPopup::onSave(CCObject* sender) {
    auto name = std::string(m_input->getString());
    if (name.empty()) {
        FLAlertLayer::create("Rename Failed", "Enter a macro name", "OK")->show();
        return;
    }

    toasty::replay::ttrl::Storage storage(toasty::replay::ttrl::defaultReplayDirectory());
    auto result = storage.rename(m_fileName, name);
    if (result.isErr()) {
        FLAlertLayer::create("Rename Failed",
                             toasty::replay::ttrl::describe(result.unwrapErr()),
                             "OK")
            ->show();
        return;
    }

    auto renamed = result.unwrap();
    if (toasty::engine::selectedReplay() == m_fileName) {
        toasty::engine::setSelectedReplay(renamed);
    }
    if (auto menu = m_menu.lock()) {
        menu->refreshMacroList(true);
    }
    this->onClose(sender);
}

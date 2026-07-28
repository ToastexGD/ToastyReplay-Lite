#include "ToastyMenu.hpp"

static constexpr float POPUP_W = 450.f;
static constexpr float POPUP_H = 290.f;
static constexpr float MARGIN = 12.f;
static constexpr float HEADER_Y = 272.f;
static constexpr float DIVIDER_Y = 256.f;
static constexpr float SIDE_X = 58.f;
static constexpr float SIDE_W = 92.f;
static constexpr float PANEL_X = 269.f;
static constexpr float PANEL_W = 314.f;
static constexpr float PANEL_LEFT = PANEL_X - PANEL_W / 2.f;
static constexpr float SCROLLBAR_X = 434.f;
static constexpr float TITLE_Y = 246.f;
static constexpr float ROW_X = 116.f;
static constexpr float ROW_W = 306.f;
static constexpr float ROW_H = 26.f;
static constexpr ccColor3B PANEL_COLOR = { 58, 29, 13 };
static constexpr ccColor3B ACCENT_COLOR = { 0, 110, 60 };

static void moveCloseTopRight(CCMenuItemSpriteExtra* closeBtn, CCNode* mainLayer, CCSize const& size) {
    closeBtn->setPosition(closeBtn->getParent()->convertToNodeSpace(
        mainLayer->convertToWorldSpace({ size.width - 8.f, size.height - 8.f })
    ));
}

static CCScale9Sprite* makeBG(CCSize size, ccColor3B color, GLubyte opacity, bool soft) {
    auto bg = CCScale9Sprite::create("square02b_001.png");
    if (soft) {
        bg->setContentSize({ size.width * 2.f, size.height * 2.f });
        bg->setScale(.5f);
    }
    else {
        bg->setContentSize(size);
    }
    bg->setColor(color);
    bg->setOpacity(opacity);
    return bg;
}

static ToastyMenu* s_instance = nullptr;
static ButtonSprite* s_captureBtn = nullptr;
static std::string s_captureId;
static std::string s_capturePrev;

static std::string keyName(enumKeyCodes key) {
    std::string name = CCKeyboardDispatcher::get()->keyToString(key);
    return name.empty() ? "?" : name;
}

static std::string modVersion() {
    return Mod::get()->getVersion().toVString();
}

static std::string modDevelopers() {
    auto devs = Mod::get()->getDevelopers();
    if (devs.empty()) return "Unknown";
    std::string out = devs.front();
    for (size_t i = 1; i < devs.size(); i++) {
        out += ", " + devs[i];
    }
    return out;
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

ToastyMenu::~ToastyMenu() {
    if (s_instance == this) {
        s_instance = nullptr;
        s_captureBtn = nullptr;
    }
}

bool ToastyMenu::init() {
    if (!Popup::init(POPUP_W, POPUP_H)) return false;

    // top right x
    moveCloseTopRight(m_closeBtn, m_mainLayer, m_size);

    this->addHeader();
    this->addSidebar();

    // main page
    {
        auto [page, menu] = this->makePage(TabMain);
        this->addPageTitle(page, "Macro Controls", nullptr);

        // disable, record, play, buttons in the "macro controlls"
        const char* modeNames[3] = { "Disable", "Record", "Play" };
        const char* modeTextures[3] = { "GJ_button_04.png", "GJ_button_06.png", "GJ_button_01.png" };
        const char* modeIds[3] = { "mode-disable", "mode-record", "mode-play" };
        for (int i = 0; i < 3; i++) {
            auto node = CCNode::create();
            node->setContentSize({ 98.f, 40.f });

            auto bg = CCScale9Sprite::create(modeTextures[i]);
            bg->setContentSize(node->getContentSize());
            bg->setPosition({ 49.f, 20.f });
            node->addChild(bg);
            m_modeBgs[i] = bg;

            auto label = CCLabelBMFont::create(modeNames[i], "bigFont.fnt");
            label->setPosition({ 49.f, 20.f });
            label->limitLabelWidth(78.f, .5f, .1f);
            node->addChild(label);
            m_modeLabels[i] = label;

            auto item = CCMenuItemSpriteExtra::create(node, this, menu_selector(ToastyMenu::onMode));
            item->setTag(i);
            item->setID(modeIds[i]);
            item->setPosition({ PANEL_LEFT + 49.f + i * 108.f, 214.f });
            menu->addChild(item);
        }

        this->addPanel(page, { PANEL_X, 104.f }, { PANEL_W, 164.f });
        auto scroll = this->addScroll(page, TabMain, { ROW_X, 26.f }, { ROW_W, 156.f });

        scroll->m_contentLayer->addChild(this->makeToggleRow("tps-bypass", "TPS Bypass", true));
        scroll->m_contentLayer->addChild(this->makeToggleRow("noclip", "Noclip", false));
        scroll->m_contentLayer->addChild(this->makeToggleRow("speedhack", "Speedhack", false));
        scroll->m_contentLayer->addChild(this->makeToggleRow("show-hitboxes", "Show Hitboxes", false));

        auto seed = this->makeRow("Set Seed", ROW_H, 100.f);
        seed.node->setID("set-seed");

        m_seedInput = TextInput::create(150.f, "Enter seed...");
        m_seedInput->setCommonFilter(CommonFilter::Uint);
        m_seedInput->setScale(.62f);
        m_seedInput->setPosition({ 180.f, ROW_H / 2.f });
        m_seedInput->setID("input");
        seed.node->addChild(m_seedInput);

        if (auto infoSpr = CCSprite::createWithSpriteFrameName("GJ_infoIcon_001.png")) {
            infoSpr->setScale(.55f);
            auto infoBtn = CCMenuItemSpriteExtra::create(infoSpr, this, menu_selector(ToastyMenu::onSeedInfo));
            infoBtn->setPosition({ 244.f, ROW_H / 2.f });
            seed.menu->addChild(infoBtn);
        }

        auto seedToggle = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(ToastyMenu::onToggleOption), .6f);
        seedToggle->setPosition({ ROW_W - 18.f, ROW_H / 2.f });
        seedToggle->setID("toggle");
        seed.menu->addChild(seedToggle);
        scroll->m_contentLayer->addChild(seed.node);

        scroll->m_contentLayer->updateLayout();
        scroll->scrollToTop();
        this->updateModes();
    }

    // macros page
    {
        auto [page, menu] = this->makePage(TabMacros);
        this->addPageTitle(page, "Macro List", nullptr);

        auto plusSpr = ButtonSprite::create("+", "bigFont.fnt", "GJ_button_01.png", .8f);
        plusSpr->setScale(.4f);
        auto plusBtn = CCMenuItemSpriteExtra::create(plusSpr, this, menu_selector(ToastyMenu::onAddMacro));
        plusBtn->setID("add-macro");
        plusBtn->setPosition({ 398.f, TITLE_Y });
        menu->addChild(plusBtn);

        if (auto refreshSpr = CCSprite::createWithSpriteFrameName("GJ_updateBtn_001.png")) {
            refreshSpr->setScale(.4f);
            auto refreshBtn = CCMenuItemSpriteExtra::create(refreshSpr, this, menu_selector(ToastyMenu::onRefreshMacros));
            refreshBtn->setID("refresh-macros");
            refreshBtn->setPosition({ 426.f, TITLE_Y });
            menu->addChild(refreshBtn);
        }

        this->addPanel(page, { PANEL_X, 129.f }, { PANEL_W, 214.f });
        auto scroll = this->addScroll(page, TabMacros, { ROW_X, 26.f }, { ROW_W, 206.f });

        const char* macroNames[] = {
            "diddy??????????", "wave spam", "SAKUPEN CIRCLES XXXX", "sync test",
            "sakupen diddy", "old", "full run backup"
        }; // honestly this is just temperary untill someone sets up the macro recording system along with whatever is needed to be implemented related to that
        for (auto name : macroNames) {
            scroll->m_contentLayer->addChild(this->makeMacroRow(name));
        }
        scroll->m_contentLayer->updateLayout();
        scroll->scrollToTop();
    }

    // settings page
    {
        auto page = this->makePage(TabSettings).node;
        this->addPageTitle(page, "Settings", nullptr);

        this->addPanel(page, { PANEL_X, 129.f }, { PANEL_W, 214.f });
        auto scroll = this->addScroll(page, TabSettings, { ROW_X, 26.f }, { ROW_W, 206.f });

        // menu scale is live so the popup can be sized before anything else exists
        auto scaleRow = this->makeRow("Menu Scale", ROW_H, 95.f);
        scaleRow.node->setID("menu-scale");

        float savedScale = Mod::get()->getSavedValue<float>("menu-scale", 1.f);

        m_scaleSlider = Slider::create(this, menu_selector(ToastyMenu::onScaleSlider), .45f);
        m_scaleSlider->setPosition({ 185.f, ROW_H / 2.f });
        m_scaleSlider->setValue((savedScale - .7f) / .4f);
        scaleRow.node->addChild(m_scaleSlider);

        m_scalePct = CCLabelBMFont::create(
            fmt::format("{}%", static_cast<int>(std::round(savedScale * 100.f))).c_str(), "bigFont.fnt"
        );
        m_scalePct->setAnchorPoint({ 1.f, .5f });
        m_scalePct->setScale(.35f);
        m_scalePct->setPosition({ ROW_W - 10.f, ROW_H / 2.f });
        scaleRow.node->addChild(m_scalePct);
        scroll->m_contentLayer->addChild(scaleRow.node);

        auto accentRow = this->makeRow("Accent Color", ROW_H, 95.f);
        accentRow.node->setID("accent-color");

        if (auto leftSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png")) {
            leftSpr->setScale(.45f);
            auto leftBtn = CCMenuItemSpriteExtra::create(leftSpr, this, menu_selector(ToastyMenu::onAccentPrev));
            leftBtn->setID("prev");
            leftBtn->setPosition({ 236.f, ROW_H / 2.f });
            accentRow.menu->addChild(leftBtn);
        }

        auto swatch = makeBG({ 15.f, 15.f }, ACCENT_COLOR, 255, true);
        swatch->setPosition({ 262.f, ROW_H / 2.f });
        accentRow.node->addChild(swatch);

        if (auto rightSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png")) {
            rightSpr->setScale(.45f);
            rightSpr->setFlipX(true);
            auto rightBtn = CCMenuItemSpriteExtra::create(rightSpr, this, menu_selector(ToastyMenu::onAccentNext));
            rightBtn->setID("next");
            rightBtn->setPosition({ 288.f, ROW_H / 2.f });
            accentRow.menu->addChild(rightBtn);
        }
        scroll->m_contentLayer->addChild(accentRow.node);

        scroll->m_contentLayer->addChild(this->makeToggleRow("show-notifications", "Show Notifications", true));
        scroll->m_contentLayer->addChild(this->makeToggleRow("remember-settings", "Remember Settings", true));
        scroll->m_contentLayer->addChild(this->makeToggleRow("auto-save-macros", "Auto Save Macros", true));
        scroll->m_contentLayer->addChild(this->makeToggleRow("check-for-updates", "Check For Updates", true));
        scroll->m_contentLayer->addChild(this->makeToggleRow("confirm-on-exit", "Confirm On Exit", false));

        scroll->m_contentLayer->updateLayout();
        scroll->scrollToTop();
    }

    // keybinds page
    {
        auto page = this->makePage(TabKeybinds).node;
        this->addPageTitle(page, "Keybinds", "Windows & macOS");

        this->addPanel(page, { PANEL_X, 129.f }, { PANEL_W, 214.f });
        auto scroll = this->addScroll(page, TabKeybinds, { ROW_X, 26.f }, { ROW_W, 206.f });

        scroll->m_contentLayer->addChild(this->makeKeybindRow("Open Menu", "key-open-menu", KEY_F8));
        scroll->m_contentLayer->addChild(this->makeKeybindRow("Record", "key-record", KEY_F1));
        scroll->m_contentLayer->addChild(this->makeKeybindRow("Replay", "key-replay", KEY_F2));
        scroll->m_contentLayer->addChild(this->makeKeybindRow("Frame Step", "key-frame-step", KEY_F3));
        scroll->m_contentLayer->addChild(this->makeKeybindRow("Speedhack", "key-speedhack", KEY_Shift));

        scroll->m_contentLayer->updateLayout();
        scroll->scrollToTop();
    }

    // about page
    {
        auto page = this->makePage(TabAbout).node;
        this->addPageTitle(page, "About", nullptr);

        this->addPanel(page, { PANEL_X, 129.f }, { PANEL_W, 214.f });
        auto scroll = this->addScroll(page, TabAbout, { ROW_X, 26.f }, { ROW_W, 206.f });

        scroll->m_contentLayer->addChild(this->makeSectionRow(Mod::get()->getName().data()));

        auto blurbRow = CCNode::create();
        blurbRow->setContentSize({ ROW_W, 40.f });
        auto blurb = SimpleTextArea::create(
            "Soo Text is just here until you guys change it :)", "chatFont.fnt", .6f, ROW_W - 20.f
        );
        blurb->setAnchorPoint({ 0.f, 1.f });
        blurb->setPosition({ 10.f, 38.f });
        blurbRow->addChild(blurb);
        scroll->m_contentLayer->addChild(blurbRow);

        scroll->m_contentLayer->addChild(this->makeSectionRow("Developer"));

        auto devRow = CCNode::create();
        devRow->setContentSize({ ROW_W, 22.f });
        auto dev = CCLabelBMFont::create(modDevelopers().c_str(), "goldFont.fnt");
        dev->setPosition({ ROW_W / 2.f, 11.f });
        dev->limitLabelWidth(ROW_W - 20.f, .5f, .1f);
        devRow->addChild(dev);
        scroll->m_contentLayer->addChild(devRow);

        scroll->m_contentLayer->addChild(this->makeSectionRow("Version"));

        auto verRow = CCNode::create();
        verRow->setContentSize({ ROW_W, 22.f });
        auto ver = CCLabelBMFont::create(modVersion().c_str(), "bigFont.fnt");
        ver->setScale(.4f);
        ver->setPosition({ ROW_W / 2.f, 11.f });
        verRow->addChild(ver);
        scroll->m_contentLayer->addChild(verRow);

        scroll->m_contentLayer->addChild(this->makeSectionRow("Credits"));

        auto creditsRow = CCNode::create();
        creditsRow->setContentSize({ ROW_W, 22.f });
        auto credits = CCLabelBMFont::create("I don't really know who all the devs are currently.", "chatFont.fnt");
        credits->setPosition({ ROW_W / 2.f, 11.f });
        credits->limitLabelWidth(ROW_W - 20.f, .55f, .1f);
        creditsRow->addChild(credits);
        scroll->m_contentLayer->addChild(creditsRow);

        scroll->m_contentLayer->updateLayout();
        scroll->scrollToTop();
    }

    static_cast<CCMenuItemToggler*>(this->getChildByIDRecursive("noclip-toggle"))->toggle(Mod::get()->getSavedValue<bool>("noclip",false));

    this->updateTabs();
    this->updatePages();

    s_instance = this;
    return true;
}

void ToastyMenu::addHeader() {
    auto title = CCLabelBMFont::create("ToastyReplay Lite", "goldFont.fnt");
    title->setAnchorPoint({ 0.f, .5f });
    title->setPosition({ 16.f, HEADER_Y });
    title->limitLabelWidth(165.f, .5f, .1f);
    m_mainLayer->addChild(title);

    auto version = CCLabelBMFont::create(modVersion().c_str(), "bigFont.fnt");
    version->setAnchorPoint({ 0.f, .5f });
    version->setScale(.3f);
    version->setPosition({ title->getPositionX() + title->getScaledContentWidth() + 12.f, HEADER_Y });
    m_mainLayer->addChild(version);

    auto divider = makeBG({ POPUP_W - MARGIN * 2.f, 2.f }, { 0, 0, 0 }, 90, false);
    divider->setPosition({ POPUP_W / 2.f, DIVIDER_Y });
    m_mainLayer->addChild(divider);
}

void ToastyMenu::addSidebar() {
    auto sidebar = makeBG({ SIDE_W, 214.f }, PANEL_COLOR, 220, false);
    sidebar->setPosition({ SIDE_X, 129.f });
    m_mainLayer->addChild(sidebar);

    auto menu = CCMenu::create();
    menu->setPosition({ 0.f, 0.f });
    menu->setContentSize(m_size);
    menu->setID("tab-menu");
    m_mainLayer->addChild(menu, 10);

    const char* tabNames[TabCount] = { "Main", "Macros", "Settings", "Keybinds", "About" };
    for (int i = 0; i < TabCount; i++) {
        auto node = CCNode::create();
        node->setContentSize({ 84.f, 26.f });

        auto bg = makeBG(node->getContentSize(), { 0, 0, 0 }, 70, true);
        bg->setPosition({ 42.f, 13.f });
        node->addChild(bg);
        m_tabBgs[i] = bg;

        auto label = CCLabelBMFont::create(tabNames[i], "bigFont.fnt");
        label->setAnchorPoint({ 0.f, .5f });
        label->setPosition({ 10.f, 13.f });
        label->limitLabelWidth(64.f, .4f, .1f);
        node->addChild(label);

        auto item = CCMenuItemSpriteExtra::create(node, this, menu_selector(ToastyMenu::onTab));
        item->setTag(i);
        item->setPosition({ SIDE_X, 215.f - i * 30.f });
        menu->addChild(item);
    }
}

ToastyMenu::Group ToastyMenu::makePage(int tab) {
    Group page;
    page.node = CCNode::create();
    page.node->setContentSize(m_size);
    m_mainLayer->addChild(page.node);
    m_pages[tab] = page.node;

    page.menu = CCMenu::create();
    page.menu->setPosition({ 0.f, 0.f });
    page.menu->setContentSize(m_size);
    page.node->addChild(page.menu);
    return page;
}

void ToastyMenu::addPageTitle(CCNode* page, const char* title, const char* hint) {
    auto label = CCLabelBMFont::create(title, "goldFont.fnt");
    label->setAnchorPoint({ 0.f, .5f });
    label->setPosition({ PANEL_LEFT, TITLE_Y });
    label->limitLabelWidth(160.f, .4f, .1f);
    page->addChild(label);

    if (!hint) return;

    auto hintLabel = CCLabelBMFont::create(hint, "chatFont.fnt");
    hintLabel->setAnchorPoint({ 1.f, .5f });
    hintLabel->setOpacity(120);
    hintLabel->setPosition({ POPUP_W - MARGIN, TITLE_Y });
    hintLabel->limitLabelWidth(130.f, .4f, .1f);
    page->addChild(hintLabel);
}

CCScale9Sprite* ToastyMenu::addPanel(CCNode* page, CCPoint center, CCSize size) {
    auto panel = makeBG(size, PANEL_COLOR, 220, false);
    panel->setPosition(center);
    page->addChild(panel);
    return panel;
}

ScrollLayer* ToastyMenu::addScroll(CCNode* page, int tab, CCPoint pos, CCSize size) {
    auto scroll = ScrollLayer::create(size);
    scroll->setPosition(pos);
    scroll->m_contentLayer->setLayout(
        ColumnLayout::create()
            ->setAxisReverse(true)
            ->setAxisAlignment(AxisAlignment::End)
            ->setAutoGrowAxis(size.height)
            ->setGap(4.f)
    );
    page->addChild(scroll);
    m_pageTouchNodes[tab].push_back(scroll);

    auto bar = Scrollbar::create(scroll);
    bar->setPosition({ SCROLLBAR_X, pos.y + size.height / 2.f });
    page->addChild(bar);
    m_pageTouchNodes[tab].push_back(bar);
    return scroll;
}

ToastyMenu::Group ToastyMenu::makeRow(const char* title, float height, float titleWidth) {
    Group row;
    row.node = CCNode::create();
    row.node->setContentSize({ ROW_W, height });

    auto bg = makeBG(row.node->getContentSize(), { 0, 0, 0 }, 45, true);
    bg->setPosition({ ROW_W / 2.f, height / 2.f });
    row.node->addChild(bg);

    auto label = CCLabelBMFont::create(title, "bigFont.fnt");
    label->setAnchorPoint({ 0.f, .5f });
    label->setPosition({ 10.f, height / 2.f });
    label->limitLabelWidth(titleWidth, .45f, .1f);
    row.node->addChild(label);

    row.menu = CCMenu::create();
    row.menu->setPosition({ 0.f, 0.f });
    row.menu->setContentSize(row.node->getContentSize());
    row.node->addChild(row.menu);
    return row;
}

CCNode* ToastyMenu::makeToggleRow(const char* id, const char* title, bool on) {
    auto row = this->makeRow(title, ROW_H, 200.f);
    row.node->setID(id);

    auto toggle = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(ToastyMenu::onToggleOption), .6f);
    toggle->setPosition({ ROW_W - 18.f, ROW_H / 2.f });
    toggle->toggle(on);
    toggle->setID(std::string(id) + "-toggle");
    row.menu->addChild(toggle);
    return row.node;
}

CCNode* ToastyMenu::makeSectionRow(const char* title) {
    auto row = CCNode::create();
    row->setContentSize({ ROW_W, 20.f });

    auto bg = makeBG(row->getContentSize(), { 0, 0, 0 }, 80, true);
    bg->setPosition({ ROW_W / 2.f, 10.f });
    row->addChild(bg);

    auto label = CCLabelBMFont::create(title, "goldFont.fnt");
    label->setPosition({ ROW_W / 2.f, 10.f });
    label->limitLabelWidth(ROW_W - 20.f, .35f, .1f);
    row->addChild(label);
    return row;
}

CCNode* ToastyMenu::makeMacroRow(const char* name) {
    auto row = CCNode::create();
    row->setContentSize({ ROW_W, 28.f });

    auto bg = makeBG(row->getContentSize(), { 0, 0, 0 }, 45, true);
    bg->setPosition({ ROW_W / 2.f, 14.f });
    row->addChild(bg);

    auto label = CCLabelBMFont::create(name, "chatFont.fnt");
    label->setAnchorPoint({ 0.f, .5f });
    label->setPosition({ 10.f, 14.f });
    label->limitLabelWidth(175.f, .65f, .1f);
    row->addChild(label);

    auto menu = CCMenu::create();
    menu->setPosition({ 0.f, 0.f });
    menu->setContentSize(row->getContentSize());
    row->addChild(menu);

    auto renameSpr = ButtonSprite::create("Edit", "bigFont.fnt", "GJ_button_04.png", .8f);
    renameSpr->setScale(.4f);
    auto renameBtn = CCMenuItemSpriteExtra::create(renameSpr, this, menu_selector(ToastyMenu::onRename));
    renameBtn->setUserObject(label);
    renameBtn->setID("rename");
    renameBtn->setPosition({ 250.f, 14.f });
    menu->addChild(renameBtn);

    auto dotsSpr = ButtonSprite::create("...", "bigFont.fnt", "GJ_button_04.png", .8f);
    dotsSpr->setScale(.4f);
    auto dotsBtn = CCMenuItemSpriteExtra::create(dotsSpr, this, menu_selector(ToastyMenu::onMacroOptions));
    dotsBtn->setID("options");
    dotsBtn->setPosition({ 285.f, 14.f });
    menu->addChild(dotsBtn);

    return row;
}

CCNode* ToastyMenu::makeKeybindRow(const char* title, const char* saveId, enumKeyCodes def) {
    auto row = this->makeRow(title, 32.f, 190.f);
    row.node->setID(saveId);

    auto saved = static_cast<enumKeyCodes>(
        Mod::get()->getSavedValue<int>(saveId, static_cast<int>(def))
    );
    auto spr = ButtonSprite::create(keyName(saved).c_str(), "goldFont.fnt", "GJ_button_04.png", .8f);
    spr->setScale(.55f);
    auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(ToastyMenu::onBindKey));
    btn->setUserObject(CCString::create(saveId));
    btn->setID("bind");
    btn->setPosition({ 272.f, 16.f });
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
        m_mainLayer->setPosition({ savedX, savedY });
    }
    this->clampMainLayer();

    m_mainLayer->setScale(0.f);
    m_mainLayer->runAction(CCEaseElasticOut::create(CCScaleTo::create(.5f, scale), .6f));

    // gd hides the cursor in levels
    PlatformToolbox::showCursor();
}

void ToastyMenu::clampMainLayer() {
    auto win = CCDirector::sharedDirector()->getWinSize();
    auto bb = m_mainLayer->boundingBox();
    float dx = 0.f, dy = 0.f;
    if (bb.getMinX() < 0.f) dx = -bb.getMinX();
    else if (bb.getMaxX() > win.width) dx = win.width - bb.getMaxX();
    if (bb.getMinY() < 0.f) dy = -bb.getMinY();
    else if (bb.getMaxY() > win.height) dy = win.height - bb.getMaxY();
    m_mainLayer->setPosition(m_mainLayer->getPosition() + CCPoint{ dx, dy });
}

bool ToastyMenu::ccTouchBegan(CCTouch* touch, CCEvent* event) {
    FLAlertLayer::ccTouchBegan(touch, event);

    CCRect grip = { 0.f, DIVIDER_Y, m_size.width, m_size.height - DIVIDER_Y };
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
    }
    else {
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

bool ToastyMenu::handleKey(enumKeyCodes key) {
    if (s_captureBtn) {
        if (key == KEY_Escape) {
            s_captureBtn->setString(s_capturePrev.c_str());
        }
        else {
            Mod::get()->setSavedValue<int>(s_captureId, static_cast<int>(key));
            s_captureBtn->setString(keyName(key).c_str());
        }
        s_captureBtn = nullptr;
        return true;
    }
    auto openKey = Mod::get()->getSavedValue<int>("key-open-menu", static_cast<int>(KEY_F8));
    if (static_cast<int>(key) == openKey) {
        if (s_instance) s_instance->onClose(nullptr);
        else ToastyMenu::create()->show();
        return true;
    }
    return false;
}

void ToastyMenu::updateModes() {
    for (int i = 0; i < 3; i++) {
        bool on = i == m_mode;
        m_modeBgs[i]->setColor(on ? ccColor3B{ 255, 255, 255 } : ccColor3B{ 95, 95, 95 });
        m_modeBgs[i]->setOpacity(on ? 255 : 190);
        m_modeLabels[i]->setColor(on ? ccColor3B{ 255, 255, 255 } : ccColor3B{ 195, 195, 195 });
    }
}

void ToastyMenu::updateTabs() {
    for (int i = 0; i < TabCount; i++) {
        if (i == m_tab) {
            m_tabBgs[i]->setColor(ACCENT_COLOR);
            m_tabBgs[i]->setOpacity(200);
        }
        else {
            m_tabBgs[i]->setColor({ 0, 0, 0 });
            m_tabBgs[i]->setOpacity(70);
        }
    }
}

void ToastyMenu::updatePages() {
    for (int i = 0; i < TabCount; i++) {
        bool visible = i == m_tab;
        m_pages[i]->setVisible(visible);
        for (auto node : m_pageTouchNodes[i]) {
            node->setTouchEnabled(visible);
        }
    }
}

void ToastyMenu::onMode(CCObject* sender) {
    m_mode = static_cast<CCNode*>(sender)->getTag();
    this->updateModes();
}

void ToastyMenu::onTab(CCObject* sender) {
    m_tab = static_cast<CCNode*>(sender)->getTag();
    this->updateTabs();
    this->updatePages();
}

void ToastyMenu::onToggleOption(CCObject* sender) {}

void ToastyMenu::onAddMacro(CCObject* sender) {}

void ToastyMenu::onRefreshMacros(CCObject* sender) {}

void ToastyMenu::onMacroOptions(CCObject* sender) {}

void ToastyMenu::onAccentPrev(CCObject* sender) {}

void ToastyMenu::onAccentNext(CCObject* sender) {}

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
    if (!spr || !id) return;
    if (s_captureBtn) s_captureBtn->setString(s_capturePrev.c_str());
    s_captureBtn = spr;
    s_captureId = id->getCString();
    s_capturePrev = spr->m_label ? spr->m_label->getString() : "";
    spr->setString("...");
}

void ToastyMenu::onClose(CCObject* sender) {
    Mod::get()->setSavedValue("noclip", static_cast<CCMenuItemToggler*>(this->getChildByIDRecursive("noclip-toggle"))->isToggled());

    if (s_instance == this) s_instance = nullptr;
    s_captureBtn = nullptr;

    // rehide cursor during gameplay
    if (PlayLayer::get()) {
        auto scene = CCDirector::sharedDirector()->getRunningScene();
        if (scene && !scene->getChildByType<PauseLayer>(0)) {
            PlatformToolbox::hideCursor();
        }
    }
    Popup::onClose(sender);
}

void ToastyMenu::onRename(CCObject* sender) {
    auto label = static_cast<CCLabelBMFont*>(static_cast<CCNode*>(sender)->getUserObject());
    if (label) RenamePopup::create(label)->show();
}

void ToastyMenu::onSeedInfo(CCObject* sender) {
    FLAlertLayer::create(
        "Set Seed",
        "placeholder",
        "OK"
    )->show();
}

RenamePopup* RenamePopup::create(CCLabelBMFont* target) {
    auto ret = new RenamePopup();
    if (ret->init(target)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool RenamePopup::init(CCLabelBMFont* target) {
    if (!Popup::init(300.f, 160.f)) return false;

    m_target = target;
    this->setTitle("Rename Macro");

    // top right x
    moveCloseTopRight(m_closeBtn, m_mainLayer, m_size);

    m_input = TextInput::create(230.f, "Macro name");
    m_input->setString(target->getString());
    m_input->setMaxCharCount(24);
    m_input->setPosition({ 150.f, 88.f });
    m_mainLayer->addChild(m_input);

    auto menu = CCMenu::create();
    menu->setPosition({ 0.f, 0.f });
    menu->setContentSize(m_size);
    m_mainLayer->addChild(menu);

    auto saveSpr = ButtonSprite::create("Save", "bigFont.fnt", "GJ_button_01.png", .8f);
    saveSpr->setScale(.7f);
    auto saveBtn = CCMenuItemSpriteExtra::create(saveSpr, this, menu_selector(RenamePopup::onSave));
    saveBtn->setPosition({ 150.f, 40.f });
    menu->addChild(saveBtn);

    return true;
}

void RenamePopup::onSave(CCObject* sender) {
    std::string str = m_input->getString();
    if (!str.empty()) {
        m_target->setString(str.c_str());
        m_target->limitLabelWidth(175.f, .65f, .1f);
    }
    this->onClose(nullptr);
}

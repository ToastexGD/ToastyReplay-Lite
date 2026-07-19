#include "ToastyMenu.hpp"

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
    if (!Popup::init(450.f, 290.f)) return false;

    // top right x
    moveCloseTopRight(m_closeBtn, m_mainLayer, m_size);

    m_menu = CCMenu::create();
    m_menu->setPosition({ 0.f, 0.f });
    m_menu->setContentSize(m_size);
    m_mainLayer->addChild(m_menu, 10);

    auto title = CCLabelBMFont::create("ToastyReplay Lite", "goldFont.fnt");
    title->setAnchorPoint({ 0.f, .5f });
    title->setPosition({ 16.f, 272.f });
    title->limitLabelWidth(165.f, .5f, .1f);
    m_mainLayer->addChild(title);

    auto version = CCLabelBMFont::create("v1.0.0", "bigFont.fnt");
    version->setAnchorPoint({ 0.f, .5f });
    version->setScale(.3f);
    version->setPosition({ 188.f, 272.f });
    m_mainLayer->addChild(version);

    auto author = CCLabelBMFont::create("by Toastexgd", "goldFont.fnt");
    author->setAnchorPoint({ 0.f, .5f });
    author->setScale(.35f);
    author->setPosition({ 236.f, 272.f });
    m_mainLayer->addChild(author);

    // sidebar
    auto sidebar = makeBG({ 92.f, 240.f }, { 58, 29, 13 }, 220, false);
    sidebar->setPosition({ 58.f, 134.f });
    m_mainLayer->addChild(sidebar);

    // sidebar tabs
    struct TabDef { const char* name; int idx; };
    std::vector<TabDef> tabs = {
        { "Main", 0 }, { "Macros", 1 }, { "Settings", 2 }, { "Keybinds", 3 }, { "About", 5 }
    };
    for (int slot = 0; slot < static_cast<int>(tabs.size()); slot++) {
        auto node = CCNode::create();
        node->setContentSize({ 84.f, 24.f });

        auto bg = makeBG({ 84.f, 24.f }, { 0, 0, 0 }, 70, true);
        bg->setPosition({ 42.f, 12.f });
        node->addChild(bg);
        m_tabBgs[tabs[slot].idx] = bg;

        auto label = CCLabelBMFont::create(tabs[slot].name, "bigFont.fnt");
        label->setAnchorPoint({ 0.f, .5f });
        label->setPosition({ 10.f, 12.f });
        label->limitLabelWidth(64.f, .4f, .1f);
        node->addChild(label);

        auto item = CCMenuItemSpriteExtra::create(node, this, menu_selector(ToastyMenu::onTab));
        item->setTag(tabs[slot].idx);
        item->setPosition({ 58.f, 240.f - slot * 28.f });
        m_menu->addChild(item);
    }




    auto makePage = [this](int idx) -> std::pair<CCNode*, CCMenu*> {
        auto page = CCNode::create();
        page->setContentSize(m_size);
        m_mainLayer->addChild(page);
        m_pages[idx] = page;

        auto menu = CCMenu::create();
        menu->setPosition({ 0.f, 0.f });
        menu->setContentSize(m_size);
        page->addChild(menu);
        return { page, menu };
    };
    auto makeScroll = [this](CCNode* page, int idx, CCPoint pos, CCSize size) -> ScrollLayer* {
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
        m_scrolls[idx].push_back(scroll);
        return scroll;
    };
    auto pageHeader = [](CCNode* page, const char* text) {
        auto label = CCLabelBMFont::create(text, "goldFont.fnt");
        label->setAnchorPoint({ 0.f, .5f });
        label->setScale(.4f);
        label->setPosition({ 114.f, 246.f });
        page->addChild(label);
    };
    auto toggleCell = [this](const char* text, bool on) -> CCNode* {
        auto cell = CCNode::create();
        cell->setContentSize({ 318.f, 26.f });

        auto bg = makeBG({ 318.f, 26.f }, { 0, 0, 0 }, 45, true);
        bg->setPosition({ 159.f, 13.f });
        cell->addChild(bg);

        auto label = CCLabelBMFont::create(text, "bigFont.fnt");
        label->setAnchorPoint({ 0.f, .5f });
        label->setPosition({ 10.f, 13.f });
        label->limitLabelWidth(200.f, .45f, .1f);
        cell->addChild(label);

        auto menu = CCMenu::create();
        menu->setPosition({ 0.f, 0.f });
        menu->setContentSize(cell->getContentSize());
        cell->addChild(menu);

        auto toggle = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(ToastyMenu::onNothing), .6f);
        toggle->setPosition({ 300.f, 13.f });
        toggle->toggle(on);
        menu->addChild(toggle);
        return cell;
    };
    auto barCell = [](const char* text) -> CCNode* {
        auto cell = CCNode::create();
        cell->setContentSize({ 318.f, 20.f });

        auto bg = makeBG({ 318.f, 20.f }, { 0, 0, 0 }, 80, true);
        bg->setPosition({ 159.f, 10.f });
        cell->addChild(bg);

        auto label = CCLabelBMFont::create(text, "goldFont.fnt");
        label->setPosition({ 159.f, 10.f });
        label->limitLabelWidth(200.f, .35f, .1f);
        cell->addChild(label);
        return cell;
    };

    // main page
    {
        auto [page, menu] = makePage(0);
        pageHeader(page, "Macro Controls");

        // disable, record, play, buttons in the "macro controlls"
        const char* modeNames[3] = { "Disable", "Record", "Play" };
        const char* modeIcons[3] = { "Macro-Disable.png"_spr, "Macro-Record.png"_spr, "Macro-Play.png"_spr };
        float modeX[3] = { 165.f, 275.f, 385.f };
        for (int i = 0; i < 3; i++) {
            auto node = CCNode::create();
            node->setContentSize({ 92.f, 58.f });

            auto bg = makeBG({ 92.f, 58.f }, { 0, 0, 0 }, 60, true);
            bg->setPosition({ 46.f, 29.f });
            node->addChild(bg);
            m_modeCards[i] = bg;

            if (auto icon = CCSprite::create(modeIcons[i])) {
                icon->setScale(30.f / std::max(icon->getContentWidth(), icon->getContentHeight()));
                icon->setPosition({ 46.f, 37.f });
                node->addChild(icon);
                m_modeIcons[i] = icon;
            }

            auto label = CCLabelBMFont::create(modeNames[i], "bigFont.fnt");
            label->setPosition({ 46.f, 12.f });
            label->limitLabelWidth(70.f, .35f, .1f);
            node->addChild(label);
            m_modeLabels[i] = label;

            auto item = CCMenuItemSpriteExtra::create(node, this, menu_selector(ToastyMenu::onMode));
            item->setTag(i);
            item->setPosition({ modeX[i], 205.f });
            menu->addChild(item);
        }
        this->updateModeCards();

        // feature list
        auto featureBG = makeBG({ 326.f, 148.f }, { 58, 29, 13 }, 220, false);
        featureBG->setPosition({ 275.f, 98.f });
        page->addChild(featureBG);

        auto scroll = makeScroll(page, 0, { 116.f, 28.f }, { 318.f, 140.f });

        // tps bypass option
        scroll->m_contentLayer->addChild(toggleCell("tps bypass", true));

        // noclip option
        scroll->m_contentLayer->addChild(toggleCell("noclip", false));

        // speedhack option
        scroll->m_contentLayer->addChild(toggleCell("speedhack", false));

        // show hitboxes option
        scroll->m_contentLayer->addChild(toggleCell("show hitboxes", false));

        // set seed option
        auto seedCell = CCNode::create();
        seedCell->setContentSize({ 318.f, 26.f });

        auto seedBG = makeBG({ 318.f, 26.f }, { 0, 0, 0 }, 45, true);
        seedBG->setPosition({ 159.f, 13.f });
        seedCell->addChild(seedBG);

        auto seedLabel = CCLabelBMFont::create("set seed", "bigFont.fnt");
        seedLabel->setAnchorPoint({ 0.f, .5f });
        seedLabel->setPosition({ 10.f, 13.f });
        seedLabel->limitLabelWidth(90.f, .45f, .1f);
        seedCell->addChild(seedLabel);

        m_seedInput = TextInput::create(160.f, "enter seed...");
        m_seedInput->setCommonFilter(CommonFilter::Uint);
        m_seedInput->setScale(.6f);
        m_seedInput->setPosition({ 168.f, 13.f });
        seedCell->addChild(m_seedInput);

        auto seedMenu = CCMenu::create();
        seedMenu->setPosition({ 0.f, 0.f });
        seedMenu->setContentSize(seedCell->getContentSize());
        seedCell->addChild(seedMenu);

        // seed info button
        if (auto infoSpr = CCSprite::createWithSpriteFrameName("GJ_infoIcon_001.png")) {
            infoSpr->setScale(.6f);
            auto infoBtn = CCMenuItemSpriteExtra::create(infoSpr, this, menu_selector(ToastyMenu::onSeedInfo));
            infoBtn->setPosition({ 240.f, 13.f });
            seedMenu->addChild(infoBtn);
        }

        auto seedToggle = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(ToastyMenu::onNothing), .6f);
        seedToggle->setPosition({ 300.f, 13.f });
        seedMenu->addChild(seedToggle);

        scroll->m_contentLayer->addChild(seedCell);
        scroll->m_contentLayer->updateLayout();
        scroll->scrollToTop();
    }

    // macros page
    {
        auto [page, menu] = makePage(1);
        pageHeader(page, "macro list");

        // add macro button
        auto plusSpr = ButtonSprite::create("+", "bigFont.fnt", "GJ_button_01.png", .8f);
        plusSpr->setScale(.4f);
        auto plusBtn = CCMenuItemSpriteExtra::create(plusSpr, this, menu_selector(ToastyMenu::onNothing));
        plusBtn->setPosition({ 400.f, 246.f });
        menu->addChild(plusBtn);

        // refresh macros button
        if (auto refreshSpr = CCSprite::createWithSpriteFrameName("GJ_updateBtn_001.png")) {
            refreshSpr->setScale(.4f);
            auto refreshBtn = CCMenuItemSpriteExtra::create(refreshSpr, this, menu_selector(ToastyMenu::onNothing));
            refreshBtn->setPosition({ 426.f, 246.f });
            menu->addChild(refreshBtn);
        }

        auto listBG = makeBG({ 326.f, 212.f }, { 58, 29, 13 }, 220, false);
        listBG->setPosition({ 275.f, 130.f });
        page->addChild(listBG);

        auto scroll = makeScroll(page, 1, { 116.f, 44.f }, { 318.f, 186.f });

        const char* macroNames[] = {
            "diddy??????????", "wave spam", "SAKUPEN CIRCLES XXXX", "sync test",
            "sakupen diddy", "old", "full run backup"
        };
        for (auto name : macroNames) {
            scroll->m_contentLayer->addChild(this->makeMacroCell(name));
        }
        scroll->m_contentLayer->updateLayout();
        scroll->scrollToTop();

        auto hint = CCLabelBMFont::create("click edit to rename", "chatFont.fnt");
        hint->setScale(.4f);
        hint->setOpacity(120);
        hint->setPosition({ 275.f, 33.f });
        page->addChild(hint);
    }

    // settings page
    {
        auto [page, menu] = makePage(2);
        pageHeader(page, "settings");

        auto bg = makeBG({ 326.f, 212.f }, { 58, 29, 13 }, 220, false);
        bg->setPosition({ 275.f, 130.f });
        page->addChild(bg);

        auto scroll = makeScroll(page, 2, { 116.f, 28.f }, { 318.f, 204.f });

        // menu scale slider
        auto sliderCell = CCNode::create();
        sliderCell->setContentSize({ 318.f, 26.f });

        auto sliderBG = makeBG({ 318.f, 26.f }, { 0, 0, 0 }, 45, true);
        sliderBG->setPosition({ 159.f, 13.f });
        sliderCell->addChild(sliderBG);

        auto sliderLabel = CCLabelBMFont::create("menu scale", "bigFont.fnt");
        sliderLabel->setAnchorPoint({ 0.f, .5f });
        sliderLabel->setPosition({ 10.f, 13.f });
        sliderLabel->limitLabelWidth(120.f, .45f, .1f);
        sliderCell->addChild(sliderLabel);

        float savedScale = Mod::get()->getSavedValue<float>("menu-scale", 1.f);

        auto slider = Slider::create(this, menu_selector(ToastyMenu::onScaleSlider), .55f);
        slider->setPosition({ 212.f, 13.f });
        slider->setValue((savedScale - .7f) / .4f);
        sliderCell->addChild(slider);
        m_scaleSlider = slider;

        auto pct = CCLabelBMFont::create(
            fmt::format("{}%", static_cast<int>(std::round(savedScale * 100.f))).c_str(), "bigFont.fnt"
        );
        pct->setAnchorPoint({ 1.f, .5f });
        pct->setScale(.4f);
        pct->setPosition({ 312.f, 13.f });
        sliderCell->addChild(pct);
        m_scalePct = pct;
        scroll->m_contentLayer->addChild(sliderCell);

        // accent color arrows
        auto accentCell = CCNode::create();
        accentCell->setContentSize({ 318.f, 26.f });

        auto accentBG = makeBG({ 318.f, 26.f }, { 0, 0, 0 }, 45, true);
        accentBG->setPosition({ 159.f, 13.f });
        accentCell->addChild(accentBG);

        auto accentLabel = CCLabelBMFont::create("accent color", "bigFont.fnt");
        accentLabel->setAnchorPoint({ 0.f, .5f });
        accentLabel->setPosition({ 10.f, 13.f });
        accentLabel->limitLabelWidth(120.f, .45f, .1f);
        accentCell->addChild(accentLabel);

        auto accentMenu = CCMenu::create();
        accentMenu->setPosition({ 0.f, 0.f });
        accentMenu->setContentSize(accentCell->getContentSize());
        accentCell->addChild(accentMenu);

        if (auto leftSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png")) {
            leftSpr->setScale(.45f);
            auto leftBtn = CCMenuItemSpriteExtra::create(leftSpr, this, menu_selector(ToastyMenu::onNothing));
            leftBtn->setPosition({ 245.f, 13.f });
            accentMenu->addChild(leftBtn);
        }

        auto swatch = makeBG({ 15.f, 15.f }, { 0, 200, 100 }, 255, true);
        swatch->setPosition({ 272.f, 13.f });
        accentCell->addChild(swatch);

        if (auto rightSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png")) {
            rightSpr->setScale(.45f);
            rightSpr->setFlipX(true);
            auto rightBtn = CCMenuItemSpriteExtra::create(rightSpr, this, menu_selector(ToastyMenu::onNothing));
            rightBtn->setPosition({ 299.f, 13.f });
            accentMenu->addChild(rightBtn);
        }
        scroll->m_contentLayer->addChild(accentCell);

        // show notifications option
        scroll->m_contentLayer->addChild(toggleCell("show notifications", true));

        // remember settings option
        scroll->m_contentLayer->addChild(toggleCell("remember settings", true));

        // auto save macros option
        scroll->m_contentLayer->addChild(toggleCell("auto save macros", true));

        // check for updates option
        scroll->m_contentLayer->addChild(toggleCell("check for updates", true));

        // confirm on exit option
        scroll->m_contentLayer->addChild(toggleCell("confirm on exit", false));

        // developer mode option
        scroll->m_contentLayer->addChild(toggleCell("developer mode", false));

        scroll->m_contentLayer->updateLayout();
        scroll->scrollToTop();
    }

    // keybinds page
    {
        auto [page, menu] = makePage(3);
        pageHeader(page, "keybinds");

        auto sub = CCLabelBMFont::create("windows & macos", "chatFont.fnt");
        sub->setAnchorPoint({ 0.f, .5f });
        sub->setScale(.4f);
        sub->setOpacity(120);
        sub->setPosition({ 114.f, 234.f });
        page->addChild(sub);

        auto bg = makeBG({ 326.f, 200.f }, { 58, 29, 13 }, 220, false);
        bg->setPosition({ 275.f, 124.f });
        page->addChild(bg);

        auto scroll = makeScroll(page, 3, { 116.f, 28.f }, { 318.f, 192.f });

        // keybind buttons
        auto keyCell = [this](const char* name, const char* saveId, enumKeyCodes def) -> CCNode* {
            auto cell = CCNode::create();
            cell->setContentSize({ 318.f, 32.f });

            auto bg = makeBG({ 318.f, 32.f }, { 0, 0, 0 }, 45, true);
            bg->setPosition({ 159.f, 16.f });
            cell->addChild(bg);

            auto label = CCLabelBMFont::create(name, "bigFont.fnt");
            label->setAnchorPoint({ 0.f, .5f });
            label->setPosition({ 10.f, 16.f });
            label->limitLabelWidth(200.f, .45f, .1f);
            cell->addChild(label);

            auto menu = CCMenu::create();
            menu->setPosition({ 0.f, 0.f });
            menu->setContentSize(cell->getContentSize());
            cell->addChild(menu);

            auto saved = static_cast<enumKeyCodes>(
                Mod::get()->getSavedValue<int>(saveId, static_cast<int>(def))
            );
            auto spr = ButtonSprite::create(keyName(saved).c_str(), "goldFont.fnt", "GJ_button_04.png", .8f);
            spr->setScale(.55f);
            auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(ToastyMenu::onBindKey));
            btn->setUserObject(CCString::create(saveId));
            btn->setPosition({ 285.f, 16.f });
            menu->addChild(btn);
            return cell;
        };
        scroll->m_contentLayer->addChild(keyCell("open menu", "key-open-menu", KEY_F8));
        scroll->m_contentLayer->addChild(keyCell("record", "key-record", KEY_F1));
        scroll->m_contentLayer->addChild(keyCell("replay", "key-replay", KEY_F2));
        scroll->m_contentLayer->addChild(keyCell("frame step", "key-frame-step", KEY_F3));
        scroll->m_contentLayer->addChild(keyCell("speedhack", "key-speedhack", KEY_Shift));
        scroll->m_contentLayer->updateLayout();
        scroll->scrollToTop();
    }

    // about page
    {
        auto [page, menu] = makePage(5);
        pageHeader(page, "about");

        auto bg = makeBG({ 326.f, 212.f }, { 58, 29, 13 }, 220, false);
        bg->setPosition({ 275.f, 130.f });
        page->addChild(bg);

        auto scroll = makeScroll(page, 5, { 116.f, 28.f }, { 318.f, 204.f });

        scroll->m_contentLayer->addChild(barCell("toastyreplay lite"));

        auto textCell = CCNode::create();
        textCell->setContentSize({ 318.f, 40.f });
        auto text = SimpleTextArea::create(
            "Soo Text is just here until you guys change it :)", "chatFont.fnt", .6f, 300.f
        );
        text->setAnchorPoint({ 0.f, 1.f });
        text->setPosition({ 8.f, 38.f });
        textCell->addChild(text);
        scroll->m_contentLayer->addChild(textCell);

        scroll->m_contentLayer->addChild(barCell("developer"));

        auto devCell = CCNode::create();
        devCell->setContentSize({ 318.f, 22.f });
        auto dev = CCLabelBMFont::create("Toastexgd", "goldFont.fnt");
        dev->setScale(.5f);
        dev->setPosition({ 159.f, 11.f });
        devCell->addChild(dev);
        scroll->m_contentLayer->addChild(devCell);

        scroll->m_contentLayer->addChild(barCell("version"));

        auto verCell = CCNode::create();
        verCell->setContentSize({ 318.f, 22.f });
        auto ver = CCLabelBMFont::create("v1.0.0", "bigFont.fnt");
        ver->setScale(.4f);
        ver->setPosition({ 159.f, 11.f });
        verCell->addChild(ver);
        scroll->m_contentLayer->addChild(verCell);

        scroll->m_contentLayer->addChild(barCell("credits"));

        auto creditsCell = CCNode::create();
        creditsCell->setContentSize({ 318.f, 22.f });
        auto credits = CCLabelBMFont::create("I don't really know who all the devs are currently.", "chatFont.fnt");
        credits->setScale(.55f);
        credits->setPosition({ 159.f, 11.f });
        creditsCell->addChild(credits);
        scroll->m_contentLayer->addChild(creditsCell);

        scroll->m_contentLayer->updateLayout();
        scroll->scrollToTop();
    }

    this->updateTabs();
    this->updatePages();

    auto footerVersion = CCLabelBMFont::create("v1.0.0", "chatFont.fnt");
    footerVersion->setAnchorPoint({ 0.f, .5f });
    footerVersion->setScale(.4f);
    footerVersion->setOpacity(150);
    footerVersion->setPosition({ 16.f, 10.f });
    m_mainLayer->addChild(footerVersion);

    s_instance = this;
    return true;
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
    auto loc = this->convertTouchToNodeSpace(touch);
    if (m_mainLayer->boundingBox().containsPoint(loc)) {
        m_dragging = true;
        m_dragOffset = m_mainLayer->getPosition() - loc;
    }
    return true;
}

void ToastyMenu::ccTouchMoved(CCTouch* touch, CCEvent* event) {
    if (m_dragging) {
        auto loc = this->convertTouchToNodeSpace(touch);
        m_mainLayer->setPosition(loc + m_dragOffset);
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

CCNode* ToastyMenu::makeMacroCell(std::string const& name) {
    auto cell = CCNode::create();
    cell->setContentSize({ 318.f, 28.f });

    auto bg = makeBG({ 318.f, 28.f }, { 0, 0, 0 }, 45, true);
    bg->setPosition({ 159.f, 14.f });
    cell->addChild(bg);

    auto label = CCLabelBMFont::create(name.c_str(), "chatFont.fnt");
    label->setAnchorPoint({ 0.f, .5f });
    label->setPosition({ 10.f, 14.f });
    label->limitLabelWidth(190.f, .65f, .1f);
    cell->addChild(label);

    auto menu = CCMenu::create();
    menu->setPosition({ 0.f, 0.f });
    menu->setContentSize(cell->getContentSize());
    cell->addChild(menu);

    // rename button
    auto renameSpr = ButtonSprite::create("edit", "bigFont.fnt", "GJ_button_04.png", .8f);
    renameSpr->setScale(.4f);
    auto renameBtn = CCMenuItemSpriteExtra::create(renameSpr, this, menu_selector(ToastyMenu::onRename));
    renameBtn->setUserObject(label);
    renameBtn->setPosition({ 266.f, 14.f });
    menu->addChild(renameBtn);

    // more options button
    auto dotsSpr = ButtonSprite::create("...", "bigFont.fnt", "GJ_button_04.png", .8f);
    dotsSpr->setScale(.4f);
    auto dotsBtn = CCMenuItemSpriteExtra::create(dotsSpr, this, menu_selector(ToastyMenu::onNothing));
    dotsBtn->setPosition({ 300.f, 14.f });
    menu->addChild(dotsBtn);

    return cell;
}

void ToastyMenu::updateModeCards() {
    // disable red, record red dot, replay green
    const ccColor3B iconColors[3] = { { 235, 60, 60 }, { 255, 80, 80 }, { 90, 220, 110 } };
    for (int i = 0; i < 3; i++) {
        bool on = i == m_mode;
        if (on) {
            m_modeCards[i]->setColor({ 0, 110, 60 });
            m_modeCards[i]->setOpacity(200);
        }
        else {
            m_modeCards[i]->setColor({ 0, 0, 0 });
            m_modeCards[i]->setOpacity(60);
        }
        if (m_modeIcons[i]) {
            m_modeIcons[i]->setColor(on ? iconColors[i] : ccColor3B{ 130, 130, 130 });
            m_modeIcons[i]->setOpacity(on ? 255 : 170);
        }
        if (m_modeLabels[i]) {
            m_modeLabels[i]->setColor(on ? ccColor3B{ 255, 255, 255 } : ccColor3B{ 190, 190, 190 });
        }
    }
}

void ToastyMenu::updateTabs() {
    for (int i = 0; i < 6; i++) {
        if (!m_tabBgs[i]) continue;
        if (i == m_tab) {
            m_tabBgs[i]->setColor({ 0, 110, 60 });
            m_tabBgs[i]->setOpacity(200);
        }
        else {
            m_tabBgs[i]->setColor({ 0, 0, 0 });
            m_tabBgs[i]->setOpacity(70);
        }
    }
}

void ToastyMenu::updatePages() {
    for (int i = 0; i < 6; i++) {
        if (!m_pages[i]) continue;
        bool visible = i == m_tab;
        m_pages[i]->setVisible(visible);
        for (auto scroll : m_scrolls[i]) {
            scroll->setTouchEnabled(visible);
        }
    }
}

void ToastyMenu::onMode(CCObject* sender) {
    m_mode = static_cast<CCNode*>(sender)->getTag();
    this->updateModeCards();
}

void ToastyMenu::onTab(CCObject* sender) {
    m_tab = static_cast<CCNode*>(sender)->getTag();
    this->updateTabs();
    this->updatePages();
}

void ToastyMenu::onNothing(CCObject* sender) {}

void ToastyMenu::onScaleSlider(CCObject* sender) {
    float scale = .7f + m_scaleSlider->getValue() * .4f;
    Mod::get()->setSavedValue<float>("menu-scale", scale);
    m_mainLayer->setScale(scale);
    if (m_scalePct) {
        m_scalePct->setString(fmt::format("{}%", static_cast<int>(std::round(scale * 100.f))).c_str());
    }
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

    m_input = TextInput::create(230.f, "macro name");
    m_input->setString(target->getString());
    m_input->setMaxCharCount(24);
    m_input->setPosition({ 150.f, 88.f });
    m_mainLayer->addChild(m_input);

    auto menu = CCMenu::create();
    menu->setPosition({ 0.f, 0.f });
    menu->setContentSize(m_size);
    m_mainLayer->addChild(menu);

    // save button
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
        m_target->limitLabelWidth(200.f, .55f, .1f);
    }
    this->onClose(nullptr);
}

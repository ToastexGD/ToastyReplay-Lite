#include "Watermark.hpp"

#include "../ToastyMenu.hpp"
#include "../engine/Engine.hpp"

#include <Geode/Geode.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/ui/Label.hpp>

#include <algorithm>

using namespace geode::prelude;

namespace {
    constexpr float IconSize = 22.f;

    bool watermarkVisible() {
        if (!Mod::get()->getSettingValue<bool>("watermark")) {
            return false;
        }
        return ToastyMenu::isOpen() || toasty::engine::recording() || toasty::engine::playing();
    }
} // namespace

namespace toasty::ui {
    void refreshWatermark() {
        auto layer = PlayLayer::get();
        if (!layer) {
            return;
        }
        if (auto node = layer->getChildByIDRecursive("watermark"_spr)) {
            node->setVisible(watermarkVisible());
        }
    }
} // namespace toasty::ui

class $modify(WatermarkPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }

        auto node = CCNode::create();
        node->setID("watermark"_spr);
        node->setPosition({10.f, 10.f});
        node->setContentSize({120.f, IconSize});

        auto label = Label::create(Mod::get()->getVersion().toVString(), "chatFont.fnt");
        label->setAnchorPoint({0.f, .5f});
        label->setScale(.5f);
        label->setOpacity(150);
        label->setPosition({IconSize + 6.f, IconSize / 2.f});
        node->addChild(label);

        if (auto icon = CCSprite::create("MenuIcon.png"_spr)) {
            icon->setScale(IconSize /
                           std::max(icon->getContentWidth(), icon->getContentHeight()));
            icon->setAnchorPoint({0.f, .5f});
            icon->setOpacity(150);
            icon->setPosition({0.f, IconSize / 2.f});
            node->addChild(icon);
        }

        this->addChild(node, 100);
        toasty::ui::refreshWatermark();
        return true;
    }
};

$on_mod(Loaded) {
    listenForSettingChanges<bool>("watermark", [](bool value) { toasty::ui::refreshWatermark(); });
}

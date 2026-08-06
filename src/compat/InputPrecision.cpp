#include <Geode/Geode.hpp>
#include <Geode/loader/GameEvent.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/modify/GameManager.hpp>

#include <string_view>

using namespace geode::prelude;

namespace {
    constexpr auto CbfModId = "syzzi.click_between_frames";
    constexpr auto CbfDisableSetting = "soft-toggle";
    constexpr auto CompatibilitySetting = "silently-disable-cbf-cbs";

    bool shouldDisableInputPrecision() {
        return Mod::get()->getSettingValue<bool>(CompatibilitySetting);
    }

    void disableClickBetweenSteps() {
        if (!shouldDisableInputPrecision()) {
            return;
        }
        if (auto manager = GameManager::get()) {
            manager->setGameVariable(GameVar::ClickBetweenSteps, false);
        }
    }

    void disableClickBetweenFrames() {
        if (!shouldDisableInputPrecision()) {
            return;
        }
        if (auto cbf = Loader::get()->getLoadedMod(CbfModId);
            cbf && !cbf->getSettingValue<bool>(CbfDisableSetting)) {
            cbf->setSettingValue<bool>(CbfDisableSetting, true);
        }
    }

    void enforceInputPrecisionSettings() {
        disableClickBetweenSteps();
        disableClickBetweenFrames();
    }
}

class $modify(ToastyReplayGameManager, GameManager) {
    void setGameVariable(char const* key, bool value) {
        auto disable = key && shouldDisableInputPrecision() &&
                       std::string_view(key) == GameVar::ClickBetweenSteps;
        GameManager::setGameVariable(key, disable ? false : value);
        if (disable) {
            if (auto layer = PlayLayer::get()) {
                layer->m_clickBetweenSteps = false;
            }
        }
    }
};

$on_mod(Loaded) {
    listenForSettingChanges<bool>(CompatibilitySetting, [](bool enabled) {
        if (enabled) {
            enforceInputPrecisionSettings();
        }
    });
}

$on_game(ModsLoaded) {
    if (auto cbf = Loader::get()->getLoadedMod(CbfModId)) {
        listenForSettingChanges<bool>(
            CbfDisableSetting,
            [](bool disabled) {
                if (!disabled) {
                    enforceInputPrecisionSettings();
                }
            },
            cbf);
    }
    disableClickBetweenFrames();
}

$on_game(Loaded) {
    enforceInputPrecisionSettings();
}

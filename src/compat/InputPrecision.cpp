#include "InputPrecision.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/GameManager.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/loader/GameEvent.hpp>
#include <Geode/loader/SettingV3.hpp>

using namespace geode::prelude;

namespace {
    constexpr auto CbfModId = "syzzi.click_between_frames";
    constexpr auto CbfDisableSetting = "soft-toggle";
    constexpr auto CompatibilitySetting = "disable-input-precision";
    constexpr auto StepsFlag = "restore-click-between-steps";
    constexpr auto FramesFlag = "restore-click-between-frames";

    bool s_active = false;
    bool s_restoreSteps = false;
    bool s_restoreFrames = false;

    void setClickBetweenSteps(bool value) {
        auto manager = GameManager::get();
        if (!manager) {
            return;
        }
        manager->setGameVariable(GameVar::ClickBetweenSteps, value);
        if (auto layer = PlayLayer::get()) {
            layer->m_clickBetweenSteps = value;
        }
    }

    void setClickBetweenFramesOff(bool value) {
        if (auto cbf = Loader::get()->getLoadedMod(CbfModId)) {
            cbf->setSettingValue<bool>(CbfDisableSetting, value);
        }
    }

    void rememberSession() {
        Mod::get()->setSavedValue<bool>(StepsFlag, s_restoreSteps);
        Mod::get()->setSavedValue<bool>(FramesFlag, s_restoreFrames);
        if (auto saved = Mod::get()->saveData(); !saved) {
            log::warn("could not save the input precision flags: {}", saved.unwrapErr());
        }
    }

    void forgetSession() {
        Mod::get()->setSavedValue<bool>(StepsFlag, false);
        Mod::get()->setSavedValue<bool>(FramesFlag, false);
    }
} // namespace

namespace toasty::compat {
    void beginSession() {
        if (s_active || !Mod::get()->getSettingValue<bool>(CompatibilitySetting)) {
            return;
        }
        s_active = true;

        auto manager = GameManager::get();
        s_restoreSteps = manager && manager->getGameVariable(GameVar::ClickBetweenSteps);
        if (s_restoreSteps) {
            setClickBetweenSteps(false);
        }

        s_restoreFrames = false;
        if (auto cbf = Loader::get()->getLoadedMod(CbfModId);
            cbf && !cbf->getSettingValue<bool>(CbfDisableSetting)) {
            cbf->setSettingValue<bool>(CbfDisableSetting, true);
            s_restoreFrames = true;
        }

        rememberSession();
    }

    void endSession() {
        if (!s_active) {
            return;
        }
        s_active = false;

        if (s_restoreSteps) {
            setClickBetweenSteps(true);
            s_restoreSteps = false;
        }

        if (s_restoreFrames) {
            setClickBetweenFramesOff(false);
            s_restoreFrames = false;
        }

        forgetSession();
    }
} // namespace toasty::compat

$on_game(Loaded) {
    if (Mod::get()->getSavedValue<bool>(StepsFlag, false)) {
        setClickBetweenSteps(true);
    }
    if (Mod::get()->getSavedValue<bool>(FramesFlag, false)) {
        setClickBetweenFramesOff(false);
    }
    forgetSession();
}

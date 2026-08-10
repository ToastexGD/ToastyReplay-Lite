#include "InputPrecision.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/GameManager.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/loader/SettingV3.hpp>

using namespace geode::prelude;

namespace {
    constexpr auto CbfModId = "syzzi.click_between_frames";
    constexpr auto CbfDisableSetting = "soft-toggle";
    constexpr auto CompatibilitySetting = "disable-input-precision";

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
            if (auto cbf = Loader::get()->getLoadedMod(CbfModId)) {
                cbf->setSettingValue<bool>(CbfDisableSetting, false);
            }
            s_restoreFrames = false;
        }
    }
} // namespace toasty::compat

#include "FrameStepper.hpp"

#include <Geode/Geode.hpp>
#include <asp/time/Instant.hpp>
#include <asp/time/Duration.hpp>
#include <Geode/binding/FMODAudioEngine.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/binding/PlayerObject.hpp>

using namespace geode::prelude;

namespace toasty::stepper {
    namespace detail {
        constexpr auto RepeatDelay = asp::time::Duration::fromMillis(300);
        constexpr auto RepeatInterval = asp::time::Duration::fromMillis(110);

        bool s_enabled = false;
        bool s_sessionOpen = false;
        bool s_keyHeld = false;
        bool s_buttonHeld = false;
        bool s_pausedMusic = false;
        int s_pending = 0;
        asp::time::Instant s_heldSince;
        asp::time::Instant s_lastRepeat;

        bool anyHeld() {
            return s_keyHeld || s_buttonHeld;
        }

        void applyHold(bool& source, bool held) {
            if (!s_enabled) {
                source = false;
                return;
            }
            auto before = anyHeld();
            source = held;
            if (!before && anyHeld()) {
                s_heldSince = asp::time::Instant::now();
                s_lastRepeat = s_heldSince;
            }
        }
    } // namespace detail

    using namespace detail;

    bool enabled() {
        return s_enabled;
    }

    void setEnabled(bool value) {
        s_enabled = value;
        s_pending = 0;
        s_keyHeld = false;
        s_buttonHeld = false;
        syncMusic();
        if (Mod::get()->getSavedValue<bool>("frame-stepper", false) != value) {
            Mod::get()->setSavedValue<bool>("frame-stepper", value);
        }
    }

    bool sessionOpen() {
        return s_sessionOpen;
    }

    void openSession() {
        s_sessionOpen = true;
        setEnabled(true);
    }

    void closeSession() {
        s_sessionOpen = false;
        setEnabled(false);
    }

    bool paused() {
        return s_sessionOpen && !s_enabled;
    }

    void setPaused(bool value) {
        if (!s_sessionOpen) {
            return;
        }
        setEnabled(!value);
    }

    void syncMusic() {
        auto engine = FMODAudioEngine::sharedEngine();
        if (!engine) {
            return;
        }
        if (s_enabled && PlayLayer::get()) {
            engine->pauseAllMusic(true);
            s_pausedMusic = true;
        } else if (s_pausedMusic) {
            engine->resumeAllMusic();
            s_pausedMusic = false;
        }
    }

    bool overridesTps() {
        return Mod::get()->getSavedValue<bool>("stepper-override-tps", false);
    }

    bool freezes() {
        if (!s_enabled) {
            return false;
        }
        auto layer = PlayLayer::get();
        if (!layer || layer->m_isPaused) {
            return false;
        }
        return !layer->m_player1 || !layer->m_player1->m_isDead;
    }

    void stepOnce() {
        if (s_enabled) {
            s_pending++;
        }
    }

    void setKeyHeld(bool held) {
        applyHold(s_keyHeld, held);
    }

    void setButtonHeld(bool held) {
        applyHold(s_buttonHeld, held);
    }

    bool takeStep() {
        if (s_pending > 0) {
            s_pending--;
            return true;
        }
        if (!anyHeld()) {
            return false;
        }
        auto now = asp::time::Instant::now();
        if (s_heldSince.elapsed() < RepeatDelay || s_lastRepeat.elapsed() < RepeatInterval) {
            return false;
        }
        s_lastRepeat = now;
        return true;
    }

} // namespace toasty::stepper

$on_mod(Loaded) {
    using namespace toasty::stepper::detail;

    s_enabled = Mod::get()->getSavedValue<bool>("frame-stepper", false);
    s_sessionOpen = s_enabled;
}

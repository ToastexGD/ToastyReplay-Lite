#include "Engine.hpp"

#include "../replay/TtrlFingerprint.hpp"

#include <algorithm>
#include <utility>

namespace toasty::engine {
    namespace {
        constexpr size_t MaximumCapturedInputs = 100000;

        Mode s_mode = Mode::Off;
        Replay s_recording;
        bool s_attemptActive = false;
        uint64_t s_maxTick = 0;
        std::string s_levelName;
        std::string s_levelData;
    } // namespace

    Mode mode() {
        return s_mode;
    }

    void setMode(Mode mode) {
        s_mode = mode;
    }

    bool recording() {
        return s_mode == Mode::Record;
    }

    void beginLevelSession(uint64_t levelId,
                           uint64_t levelRevision,
                           std::string levelName,
                           std::string levelData) {
        s_recording = {};
        s_recording.levelId = levelId;
        s_recording.levelRevision = levelRevision;
        s_levelName = std::move(levelName);
        s_levelData = std::move(levelData);
        s_attemptActive = false;
        s_maxTick = 0;
    }

    void beginAttempt() {
        s_attemptActive = true;
    }

    void endAttempt() {
        s_attemptActive = false;
    }

    void cancelAttempt() {
        s_attemptActive = false;
    }

    void capture(uint64_t tick, InputButton button, InputPlayer player, bool pressed) {
        if (s_recording.inputs.size() >= MaximumCapturedInputs) {
            return;
        }
        s_recording.inputs.push_back({tick, button, player, pressed});
        s_maxTick = std::max(s_maxTick, tick);
    }

    std::optional<Replay> takeRecording() {
        if (s_recording.inputs.empty()) {
            return std::nullopt;
        }
        s_recording.tickCount = s_maxTick + 1;
        s_recording.levelFingerprint = toasty::replay::ttrl::fingerprintLevelData(s_levelData);
        auto result = std::move(s_recording);
        s_recording = {};
        s_maxTick = 0;
        return result;
    }

    std::string const& levelName() {
        return s_levelName;
    }
} // namespace toasty::engine

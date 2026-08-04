#include "Engine.hpp"

#include <algorithm>

namespace toasty::engine {
    namespace {
        constexpr size_t MaximumCapturedInputs = 100000;

        Mode s_mode = Mode::Off;
        Replay s_recording;
        bool s_attemptActive = false;
        uint64_t s_maxTick = 0;
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

    void beginLevelSession(uint64_t levelId, uint64_t levelRevision) {
        s_recording = {};
        s_recording.levelId = levelId;
        s_recording.levelRevision = levelRevision;
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
        auto result = std::move(s_recording);
        s_recording = {};
        s_maxTick = 0;
        return result;
    }
} // namespace toasty::engine

#pragma once

#include "../replay/TtrlReplay.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace toasty::engine {
    using toasty::replay::InputButton;
    using toasty::replay::InputPlayer;
    using toasty::replay::Replay;

    enum class Mode : uint8_t { Off, Record, Play };

    struct Session {
        Mode mode = Mode::Off;
        uint64_t levelId = 0;
        uint64_t levelRevision = 0;
        std::string levelName;
        std::string levelData;
        bool platformer = false;
        Replay recording;
        std::optional<Replay> playback;
        size_t nextInput = 0;
        size_t nextFix = 0;
        uint64_t tick = 0;
        int64_t recordingRate = 240;
        bool processingTick = false;
        bool acceptingPlaybackInput = false;
        bool resetting = false;
        bool previousTestMode = false;
        bool changedTestMode = false;
        bool tpsOverride = false;
        std::array<bool, 6> heldInputs = {};

        ~Session();
    };

    Session* activeSession();

    Mode mode();
    bool recording();
    bool playing();
    bool startRecording();
    bool stopRecording(bool save);
    bool loadReplayAndBegin(std::string name);
    void togglePlayback();
    void stopPlayback();

    void setSelectedReplay(std::string name);
    std::string const& selectedReplay();
} // namespace toasty::engine

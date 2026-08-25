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
        std::optional<float> playbackHold;
        std::optional<float> playbackHoldArm;
        uint64_t playbackSeekTick = 0;
        size_t playbackSeekInput = 0;
        size_t playbackSeekFix = 0;
        std::array<bool, 6> playbackSeekHeld = {};
        float playbackAlignedStart = 0.f;
        bool pendingHeld = false;
        bool previousTestMode = false;
        bool changedTestMode = false;
        bool tpsOverride = false;
        bool capturing = false;
        bool captureComplete = false;
        std::array<bool, 6> heldInputs = {};

        ~Session();
    };

    Session* activeSession();

    Mode mode();
    bool recording();
    bool playing();
    bool startRecording();
    bool stopRecording(bool save);
    bool loadReplayAndBegin(std::string name, bool capturing = false);
    bool convertToFrameFixes(std::string name);
    void finishConversion();
    void togglePlayback();
    void stopPlayback();
    bool realignPlayback(PlayLayer* layer);

    void setSelectedReplay(std::string name);
    std::string const& selectedReplay();
} // namespace toasty::engine

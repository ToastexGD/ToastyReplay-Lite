#pragma once

#include "../replay/TtrlReplay.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace toasty::engine {
    using toasty::replay::InputButton;
    using toasty::replay::InputPlayer;
    using toasty::replay::Replay;

    enum class Mode : uint8_t { Off, Record, Play };

    Mode mode();
    void setMode(Mode mode);
    bool recording();

    void beginLevelSession(uint64_t levelId,
                           uint64_t levelRevision,
                           std::string levelName,
                           std::string levelData);
    void beginAttempt();
    void endAttempt();
    void cancelAttempt();

    void capture(uint64_t tick, InputButton button, InputPlayer player, bool pressed);
    std::optional<Replay> takeRecording();
    std::string const& levelName();

    void saveRecordingIfAny();
} // namespace toasty::engine

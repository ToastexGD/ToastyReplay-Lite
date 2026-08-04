#pragma once

#include "../replay/TtrlReplay.hpp"

#include <cstdint>
#include <optional>

namespace toasty::engine {
    using toasty::replay::InputButton;
    using toasty::replay::InputPlayer;
    using toasty::replay::Replay;

    enum class Mode : uint8_t { Off, Record, Play };

    Mode mode();
    void setMode(Mode mode);
    bool recording();

    void beginLevelSession(uint64_t levelId, uint64_t levelRevision);
    void beginAttempt();
    void endAttempt();
    void cancelAttempt();

    void capture(uint64_t tick, InputButton button, InputPlayer player, bool pressed);
    std::optional<Replay> takeRecording();
} // namespace toasty::engine

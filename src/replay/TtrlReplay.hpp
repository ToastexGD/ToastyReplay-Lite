#pragma once

#include <cstdint>
#include <vector>

namespace toasty::replay {
    struct TpsRate {
        uint64_t numerator = 240;
        uint64_t denominator = 1;

        bool operator==(TpsRate const&) const = default;
    };

    enum class InputButton : uint8_t {
        Jump = 1,
        Left = 2,
        Right = 3
    };

    enum class InputPlayer : uint8_t {
        Player1 = 1,
        Player2 = 2
    };

    struct InputEvent {
        uint64_t tick = 0;
        InputButton button = InputButton::Jump;
        InputPlayer player = InputPlayer::Player1;
        bool pressed = false;

        bool operator==(InputEvent const&) const = default;
    };

    struct FrameFix {
        uint64_t tick = 0;
        float x = 0.f;
        float y = 0.f;
        float rotation = 0.f;
        double verticalVelocity = 0.0;

        bool operator==(FrameFix const&) const = default;
    };

    struct Replay {
        TpsRate tps;
        uint32_t gameVersion = 22081;
        uint64_t levelId = 0;
        uint64_t levelRevision = 0;
        uint64_t levelFingerprint = 0;
        uint64_t durationTicks = 0;
        std::vector<InputEvent> inputs;
        std::vector<FrameFix> frameFixes;

        bool operator==(Replay const&) const = default;
    };
}

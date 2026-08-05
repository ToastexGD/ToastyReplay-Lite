#pragma once

#include <cstdint>
#include <numeric>
#include <optional>
#include <vector>

namespace toasty::replay {
    struct TpsRate {
        uint64_t numerator = 240;
        uint64_t denominator = 1;

        std::optional<TpsRate> normalized() const {
            if (numerator == 0 || denominator == 0)
                return std::nullopt;
            auto divisor = std::gcd(numerator, denominator);
            return TpsRate{numerator / divisor, denominator / divisor};
        }

        bool operator==(TpsRate const&) const = default;
    };

    enum class PlayMode : uint8_t { Normal, Platformer };

    enum class InputButton : uint8_t { Jump = 1, Left = 2, Right = 3 };

    enum class InputPlayer : uint8_t { Player1 = 1, Player2 = 2 };

    struct InputEvent {
        uint64_t beforeTick = 0;
        InputButton button = InputButton::Jump;
        InputPlayer player = InputPlayer::Player1;
        bool pressed = false;

        bool operator==(InputEvent const&) const = default;
    };

    struct FrameFix {
        uint64_t afterTick = 0;
        InputPlayer player = InputPlayer::Player1;
        float x = 0.f;
        float y = 0.f;
        float rotation = 0.f;
        double verticalVelocity = 0.0;

        bool operator==(FrameFix const&) const = default;
    };

    struct Replay {
        TpsRate tps;
        PlayMode mode = PlayMode::Normal;
        uint32_t gameVersion = 22081;
        uint64_t levelId = 0;
        uint64_t levelRevision = 0;
        uint64_t levelFingerprint = 0;
        uint64_t tickCount = 0;
        std::vector<InputEvent> inputs;
        std::vector<FrameFix> frameFixes;

        bool operator==(Replay const&) const = default;
    };
} // namespace toasty::replay

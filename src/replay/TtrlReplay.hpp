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

    enum class PlayerState : uint16_t {
        Ship = 1 << 0,
        Bird = 1 << 1,
        Ball = 1 << 2,
        Dart = 1 << 3,
        Robot = 1 << 4,
        Spider = 1 << 5,
        Swing = 1 << 6,
        UpsideDown = 1 << 7,
        Sideways = 1 << 8
    };

    struct FrameFix {
        uint64_t afterTick = 0;
        InputPlayer player = InputPlayer::Player1;
        float x = 0.f;
        float y = 0.f;
        float rotation = 0.f;
        double verticalVelocity = 0.0;
        uint16_t state = 0;
        float vehicleSize = 1.f;
        float playerSpeed = 1.f;
        float gravityMod = 1.f;

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
        std::optional<uint64_t> seed;
        std::optional<float> startPos;
        std::optional<bool> controlFlip;
        std::vector<InputEvent> inputs;
        std::vector<FrameFix> frameFixes;

        bool operator==(Replay const&) const = default;
    };
} // namespace toasty::replay

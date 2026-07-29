#pragma once

#include "TtrlReplay.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace toasty::replay::ttrl {
    enum class CodecError {
        FileTooSmall,
        FileTooLarge,
        ChecksumMismatch,
        InvalidMagic,
        UnsupportedVersion,
        UnsupportedFlags,
        UnsupportedFrameFixSchema,
        Truncated,
        InvalidVarint,
        InvalidTps,
        InvalidGameVersion,
        InvalidInputOrder,
        InvalidFrameFix,
        TickOverflow,
        TickPastDuration,
        TrailingData
    };

    struct CodecFailure {
        CodecError error;
        size_t offset;

        bool operator==(CodecFailure const&) const = default;
    };

    using EncodeResult = std::expected<std::vector<uint8_t>, CodecFailure>;
    using DecodeResult = std::expected<Replay, CodecFailure>;

    EncodeResult encode(Replay const& replay);
    DecodeResult decode(std::span<uint8_t const> bytes);
}

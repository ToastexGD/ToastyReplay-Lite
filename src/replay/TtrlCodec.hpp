#pragma once

#include "TtrlReplay.hpp"

#include <Geode/Result.hpp>
#include <Geode/DefaultInclude.hpp>

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace toasty::replay::ttrl {
    constexpr size_t MaximumFileSize = 64 * 1024 * 1024;
    constexpr size_t MaximumInputSize = 8 * 1024 * 1024;
    constexpr size_t MaximumInputEvents = 1024 * 1024;
    constexpr size_t MaximumFrameFixes = 1024 * 1024;

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
        InvalidInput,
        InvalidInputOrder,
        InvalidFrameFix,
        TooManyInputs,
        TooManyFrameFixes,
        TickOverflow,
        TickOutsideReplay,
        TrailingData
    };

    struct CodecFailure {
        CodecError error;
        size_t offset;

        bool operator==(CodecFailure const&) const = default;
    };

    using EncodeResult = geode::Result<geode::ByteVector, CodecFailure>;
    using DecodeResult = geode::Result<Replay, CodecFailure>;

    EncodeResult encode(Replay const& replay);
    DecodeResult decode(geode::ByteSpan bytes);
    std::string_view errorMessage(CodecError error);
} // namespace toasty::replay::ttrl

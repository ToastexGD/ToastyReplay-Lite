#include "TtrlCodec.hpp"

#include <asp/iter.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <utility>

using namespace geode::prelude;

namespace toasty::replay::ttrl::codec {
    using toasty::replay::FrameFix;
    using toasty::replay::InputButton;
    using toasty::replay::InputEvent;
    using toasty::replay::InputPlayer;
    using toasty::replay::PlayMode;
    using toasty::replay::Replay;
    using toasty::replay::TpsRate;

    constexpr std::array<uint8_t, 4> Magic = {'T', 'T', 'R', 'L'};
    constexpr uint8_t Version = 1;
    constexpr uint8_t FrameFixFlag = 1;
    constexpr uint8_t InputChannelsFlag = 2;
    constexpr uint8_t PlatformerFlag = 4;
    constexpr uint8_t SeedFlag = 8;
    constexpr uint8_t StartPosFlag = 16;
    constexpr uint8_t KnownFlags =
        FrameFixFlag | InputChannelsFlag | PlatformerFlag | SeedFlag | StartPosFlag;
    constexpr uint8_t LegacyFrameFixSchema = 1;
    constexpr uint8_t PositionFrameFixSchema = 2;
    constexpr uint8_t FrameFixSchema = 3;
    constexpr size_t MinimumFileSize = 25;
    constexpr size_t MaximumFrameFixSize = MaximumFileSize - MaximumInputSize;
    constexpr size_t MinimumFrameFixRecordSize = 21;

    static impl::ErrContainer<CodecFailure> failure(CodecError error, size_t offset) {
        return Err(CodecFailure{error, offset});
    }

    bool validInputButton(InputButton button) {
        return button == InputButton::Jump || button == InputButton::Left ||
               button == InputButton::Right;
    }

    bool validInputPlayer(InputPlayer player) {
        return player == InputPlayer::Player1 || player == InputPlayer::Player2;
    }

    bool usesInputChannels(Replay const& replay) {
        return asp::iter::from(replay.inputs).any([](auto const& event) {
            return event.button != InputButton::Jump || event.player != InputPlayer::Player1;
        });
    }

    void appendVarint(ByteVector& bytes, uint64_t value) {
        do {
            auto byte = static_cast<uint8_t>(value & 0x7f);
            value >>= 7;
            if (value != 0)
                byte |= 0x80;
            bytes.push_back(byte);
        } while (value != 0);
    }

    void append32(ByteVector& bytes, uint32_t value) {
        bytes.push_back(static_cast<uint8_t>(value));
        bytes.push_back(static_cast<uint8_t>(value >> 8));
        bytes.push_back(static_cast<uint8_t>(value >> 16));
        bytes.push_back(static_cast<uint8_t>(value >> 24));
    }

    void append64(ByteVector& bytes, uint64_t value) {
        for (uint32_t shift = 0; shift < 64; shift += 8) {
            bytes.push_back(static_cast<uint8_t>(value >> shift));
        }
    }

    void appendFloat(ByteVector& bytes, float value) {
        append32(bytes, std::bit_cast<uint32_t>(value));
    }

    void appendDouble(ByteVector& bytes, double value) {
        append64(bytes, std::bit_cast<uint64_t>(value));
    }

    uint32_t checksum(ByteSpan bytes) {
        uint32_t value = 0xffffffff;
        for (uint8_t byte : asp::iter::from(bytes)) {
            value ^= byte;
            for (uint32_t bit = 0; bit < 8; ++bit) {
                auto mask = 0u - (value & 1u);
                value = (value >> 1) ^ (0xedb88320u & mask);
            }
        }
        return ~value;
    }

    class Reader {
      public:
        explicit Reader(ByteSpan bytes)
            : m_bytes(bytes) {}

        Result<uint8_t, CodecFailure> readByte() {
            if (m_position >= m_bytes.size()) {
                return failure(CodecError::Truncated, m_position);
            }
            return Ok(m_bytes[m_position++]);
        }

        Result<uint64_t, CodecFailure> readVarint() {
            auto start = m_position;
            uint64_t value = 0;

            for (uint32_t shift = 0; shift <= 63; shift += 7) {
                GEODE_UNWRAP_INTO(auto byte, readByte());

                if (shift == 63 && (byte & 0xfe) != 0) {
                    return failure(CodecError::InvalidVarint, start);
                }

                value |= static_cast<uint64_t>(byte & 0x7f) << shift;
                if ((byte & 0x80) == 0) {
                    if (shift != 0 && byte == 0) {
                        return failure(CodecError::InvalidVarint, start);
                    }
                    return Ok(value);
                }
            }

            return failure(CodecError::InvalidVarint, start);
        }

        Result<uint32_t, CodecFailure> read32() {
            GEODE_UNWRAP_INTO(auto bytes, readSpan(4));
            return Ok(static_cast<uint32_t>(bytes[0]) |
                      (static_cast<uint32_t>(bytes[1]) << 8) |
                      (static_cast<uint32_t>(bytes[2]) << 16) |
                      (static_cast<uint32_t>(bytes[3]) << 24));
        }

        Result<uint64_t, CodecFailure> read64() {
            GEODE_UNWRAP_INTO(auto bytes, readSpan(8));
            uint64_t value = 0;
            for (uint32_t shift = 0; shift < 64; shift += 8) {
                value |= static_cast<uint64_t>(bytes[shift / 8]) << shift;
            }
            return Ok(value);
        }

        Result<float, CodecFailure> readFloat() {
            GEODE_UNWRAP_INTO(auto value, read32());
            return Ok(std::bit_cast<float>(value));
        }

        Result<double, CodecFailure> readDouble() {
            GEODE_UNWRAP_INTO(auto value, read64());
            return Ok(std::bit_cast<double>(value));
        }

        Result<ByteSpan, CodecFailure> readSpan(size_t size) {
            if (size > m_bytes.size() - m_position) {
                return failure(CodecError::Truncated, m_position);
            }

            auto result = m_bytes.subspan(m_position, size);
            m_position += size;
            return Ok(result);
        }

        size_t position() const {
            return m_position;
        }

        size_t remaining() const {
            return m_bytes.size() - m_position;
        }

        bool finished() const {
            return m_position == m_bytes.size();
        }

      private:
        ByteSpan m_bytes;
        size_t m_position = 0;
    };

    Result<uint64_t, CodecFailure>
    checkedTick(uint64_t previous, uint64_t delta, uint64_t tickCount, size_t offset) {
        if (delta > std::numeric_limits<uint64_t>::max() - previous) {
            return failure(CodecError::TickOverflow, offset);
        }

        auto tick = previous + delta;
        if (tick >= tickCount) {
            return failure(CodecError::TickOutsideReplay, offset);
        }
        return Ok(tick);
    }

    Result<FrameFix, CodecFailure>
    readFrameFix(Reader& reader,
                 uint8_t schema,
                 uint64_t previousTick,
                 uint64_t tickCount) {
        auto offset = reader.position();
        GEODE_UNWRAP_INTO(auto value, reader.readVarint());
        auto player = InputPlayer::Player1;
        auto delta = value;
        if (schema != LegacyFrameFixSchema) {
            player = (value & 1) != 0 ? InputPlayer::Player2 : InputPlayer::Player1;
            delta = value >> 1;
        }
        GEODE_UNWRAP_INTO(auto tick, checkedTick(previousTick, delta, tickCount, offset));
        GEODE_UNWRAP_INTO(auto x, reader.readFloat());
        GEODE_UNWRAP_INTO(auto y, reader.readFloat());
        GEODE_UNWRAP_INTO(auto rotation, reader.readFloat());
        GEODE_UNWRAP_INTO(auto verticalVelocity, reader.readDouble());

        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(rotation) ||
            !std::isfinite(verticalVelocity)) {
            return failure(CodecError::InvalidFrameFix, offset);
        }

        uint16_t state = 0;
        float vehicleSize = 1.f;
        float playerSpeed = 1.f;
        float gravityMod = 1.f;
        if (schema == FrameFixSchema) {
            GEODE_UNWRAP_INTO(auto stateValue, reader.readVarint());
            if (stateValue > std::numeric_limits<uint16_t>::max()) {
                return failure(CodecError::InvalidFrameFix, offset);
            }
            state = static_cast<uint16_t>(stateValue);
            GEODE_UNWRAP_INTO(vehicleSize, reader.readFloat());
            GEODE_UNWRAP_INTO(playerSpeed, reader.readFloat());
            GEODE_UNWRAP_INTO(gravityMod, reader.readFloat());
            if (!std::isfinite(vehicleSize) || !std::isfinite(playerSpeed) ||
                !std::isfinite(gravityMod)) {
                return failure(CodecError::InvalidFrameFix, offset);
            }
        }

        return Ok(FrameFix{.afterTick = tick,
                           .player = player,
                           .x = x,
                           .y = y,
                           .rotation = rotation,
                           .verticalVelocity = verticalVelocity,
                           .state = state,
                           .vehicleSize = vehicleSize,
                           .playerSpeed = playerSpeed,
                           .gravityMod = gravityMod});
    }

    Result<TpsRate, CodecFailure> validate(Replay const& replay, bool inputChannels) {
        auto tps = replay.tps.normalized();
        if (!tps) {
            return failure(CodecError::InvalidTps, 0);
        }

        if (replay.gameVersion == 0) {
            return failure(CodecError::InvalidGameVersion, 0);
        }
        if (replay.inputs.size() > MaximumInputEvents) {
            return failure(CodecError::TooManyInputs, replay.inputs.size());
        }
        if (replay.frameFixes.size() > MaximumFrameFixes) {
            return failure(CodecError::TooManyFrameFixes, replay.frameFixes.size());
        }

        uint64_t previousTick = 0;
        for (auto [index, eventRef] : asp::iter::enumerate(replay.inputs)) {
            InputEvent const& event = eventRef;
            if (!validInputButton(event.button) || !validInputPlayer(event.player)) {
                return failure(CodecError::InvalidInput, index);
            }
            if (index != 0 && event.beforeTick < previousTick) {
                return failure(CodecError::InvalidInputOrder, index);
            }
            if (event.beforeTick >= replay.tickCount) {
                return failure(CodecError::TickOutsideReplay, index);
            }
            auto shift = inputChannels ? 4 : 1;
            if (event.beforeTick - previousTick > (std::numeric_limits<uint64_t>::max() >> shift)) {
                return failure(CodecError::TickOverflow, index);
            }
            previousTick = event.beforeTick;
        }

        previousTick = 0;
        for (auto [index, fixRef] : asp::iter::enumerate(replay.frameFixes)) {
            FrameFix const& fix = fixRef;
            if (index != 0 && fix.afterTick < previousTick) {
                return failure(CodecError::InvalidFrameFix, index);
            }
            if (fix.afterTick >= replay.tickCount) {
                return failure(CodecError::TickOutsideReplay, index);
            }
            if (!std::isfinite(fix.x) || !std::isfinite(fix.y) || !std::isfinite(fix.rotation) ||
                !std::isfinite(fix.verticalVelocity)) {
                return failure(CodecError::InvalidFrameFix, index);
            }
            if (!validInputPlayer(fix.player)) {
                return failure(CodecError::InvalidFrameFix, index);
            }
            if (fix.afterTick - previousTick > (std::numeric_limits<uint64_t>::max() >> 1)) {
                return failure(CodecError::TickOverflow, index);
            }
            previousTick = fix.afterTick;
        }

        return Ok(*tps);
    }

    EncodeResult encode(Replay const& replay) {
        auto inputChannels = usesInputChannels(replay);
        GEODE_UNWRAP_INTO(auto tps, validate(replay, inputChannels));

        ByteVector inputBytes;
        uint64_t previousTick = 0;
        for (auto const& eventRef : asp::iter::from(replay.inputs)) {
            InputEvent const& event = eventRef;
            if (inputBytes.size() > MaximumInputSize - 10) {
                return failure(CodecError::FileTooLarge, inputBytes.size());
            }
            auto delta = event.beforeTick - previousTick;
            if (inputChannels) {
                auto state = ((static_cast<uint64_t>(event.button) - 1) << 2) |
                             (event.player == InputPlayer::Player2 ? 2 : 0) |
                             static_cast<uint64_t>(event.pressed);
                appendVarint(inputBytes, (delta << 4) | state);
            } else {
                appendVarint(inputBytes, (delta << 1) | static_cast<uint64_t>(event.pressed));
            }
            previousTick = event.beforeTick;
        }

        ByteVector frameFixBytes;
        if (!replay.frameFixes.empty()) {
            appendVarint(frameFixBytes, replay.frameFixes.size());
            previousTick = 0;
            for (auto const& fixRef : asp::iter::from(replay.frameFixes)) {
                FrameFix const& fix = fixRef;
                if (frameFixBytes.size() > MaximumFrameFixSize - 60) {
                    return failure(CodecError::FileTooLarge, frameFixBytes.size());
                }
                auto delta = fix.afterTick - previousTick;
                auto player = fix.player == InputPlayer::Player2 ? 1u : 0u;
                appendVarint(frameFixBytes, (delta << 1) | player);
                appendFloat(frameFixBytes, fix.x);
                appendFloat(frameFixBytes, fix.y);
                appendFloat(frameFixBytes, fix.rotation);
                appendDouble(frameFixBytes, fix.verticalVelocity);
                appendVarint(frameFixBytes, fix.state);
                appendFloat(frameFixBytes, fix.vehicleSize);
                appendFloat(frameFixBytes, fix.playerSpeed);
                appendFloat(frameFixBytes, fix.gravityMod);
                previousTick = fix.afterTick;
            }
        }

        ByteVector bytes;
        bytes.insert(bytes.end(), Magic.begin(), Magic.end());
        bytes.push_back(Version);
        uint8_t flags = 0;
        if (!replay.frameFixes.empty())
            flags |= FrameFixFlag;
        if (inputChannels)
            flags |= InputChannelsFlag;
        if (replay.mode == PlayMode::Platformer)
            flags |= PlatformerFlag;
        if (replay.seed)
            flags |= SeedFlag;
        if (replay.startPos)
            flags |= StartPosFlag;
        bytes.push_back(flags);
        appendVarint(bytes, tps.numerator);
        appendVarint(bytes, tps.denominator);
        appendVarint(bytes, replay.gameVersion);
        appendVarint(bytes, replay.levelId);
        appendVarint(bytes, replay.levelRevision);
        append64(bytes, replay.levelFingerprint);
        appendVarint(bytes, replay.tickCount);
        if (replay.seed)
            appendVarint(bytes, *replay.seed);
        if (replay.startPos)
            appendFloat(bytes, *replay.startPos);
        appendVarint(bytes, inputBytes.size());
        bytes.insert(bytes.end(), inputBytes.begin(), inputBytes.end());

        if (!frameFixBytes.empty()) {
            bytes.push_back(FrameFixSchema);
            appendVarint(bytes, frameFixBytes.size());
            bytes.insert(bytes.end(), frameFixBytes.begin(), frameFixBytes.end());
        }

        if (bytes.size() > MaximumFileSize - 4) {
            return failure(CodecError::FileTooLarge, bytes.size());
        }

        append32(bytes, checksum(bytes));
        return Ok(std::move(bytes));
    }

    DecodeResult decode(ByteSpan bytes) {
        if (bytes.size() < MinimumFileSize) {
            return failure(CodecError::FileTooSmall, bytes.size());
        }
        if (bytes.size() > MaximumFileSize) {
            return failure(CodecError::FileTooLarge, bytes.size());
        }

        auto payloadSize = bytes.size() - 4;
        auto checksumBytes = bytes.subspan(payloadSize);
        auto storedChecksum = static_cast<uint32_t>(checksumBytes[0]) |
                              (static_cast<uint32_t>(checksumBytes[1]) << 8) |
                              (static_cast<uint32_t>(checksumBytes[2]) << 16) |
                              (static_cast<uint32_t>(checksumBytes[3]) << 24);

        if (checksum(bytes.first(payloadSize)) != storedChecksum) {
            return failure(CodecError::ChecksumMismatch, payloadSize);
        }

        Reader reader(bytes.first(payloadSize));
        GEODE_UNWRAP_INTO(auto magic, reader.readSpan(Magic.size()));
        if (!std::equal(magic.begin(), magic.end(), Magic.begin())) {
            return failure(CodecError::InvalidMagic, 0);
        }

        GEODE_UNWRAP_INTO(auto version, reader.readByte());
        if (version != Version) {
            return failure(CodecError::UnsupportedVersion, reader.position() - 1);
        }

        GEODE_UNWRAP_INTO(auto flags, reader.readByte());
        if ((flags & ~KnownFlags) != 0) {
            return failure(CodecError::UnsupportedFlags, reader.position() - 1);
        }
        auto inputChannels = (flags & InputChannelsFlag) != 0;

        Replay replay;
        replay.mode =
            (flags & PlatformerFlag) != 0 ? PlayMode::Platformer : PlayMode::Normal;

        GEODE_UNWRAP_INTO(auto numerator, reader.readVarint());
        GEODE_UNWRAP_INTO(auto denominator, reader.readVarint());
        auto tps = TpsRate{numerator, denominator};
        auto normalizedTps = tps.normalized();
        if (!normalizedTps || *normalizedTps != tps) {
            return failure(CodecError::InvalidTps, reader.position());
        }
        replay.tps = tps;

        GEODE_UNWRAP_INTO(auto gameVersion, reader.readVarint());
        if (gameVersion == 0 || gameVersion > std::numeric_limits<uint32_t>::max()) {
            return failure(CodecError::InvalidGameVersion, reader.position());
        }
        replay.gameVersion = static_cast<uint32_t>(gameVersion);

        GEODE_UNWRAP_INTO(auto levelId, reader.readVarint());
        replay.levelId = levelId;

        GEODE_UNWRAP_INTO(auto levelRevision, reader.readVarint());
        replay.levelRevision = levelRevision;

        GEODE_UNWRAP_INTO(auto levelFingerprint, reader.read64());
        replay.levelFingerprint = levelFingerprint;

        GEODE_UNWRAP_INTO(auto tickCount, reader.readVarint());
        replay.tickCount = tickCount;

        if ((flags & SeedFlag) != 0) {
            GEODE_UNWRAP_INTO(auto seed, reader.readVarint());
            replay.seed = seed;
        }

        if ((flags & StartPosFlag) != 0) {
            GEODE_UNWRAP_INTO(auto startPos, reader.readFloat());
            replay.startPos = startPos;
        }

        GEODE_UNWRAP_INTO(auto inputSize, reader.readVarint());
        if (inputSize > MaximumInputSize) {
            return failure(CodecError::FileTooLarge, reader.position());
        }
        if (inputSize > reader.remaining()) {
            return failure(CodecError::Truncated, reader.position());
        }

        GEODE_UNWRAP_INTO(auto inputSpan, reader.readSpan(static_cast<size_t>(inputSize)));
        Reader inputReader(inputSpan);

        uint64_t previousTick = 0;
        while (!inputReader.finished()) {
            if (replay.inputs.size() == MaximumInputEvents) {
                return failure(CodecError::TooManyInputs, inputReader.position());
            }
            auto offset = inputReader.position();
            GEODE_UNWRAP_INTO(auto event, inputReader.readVarint());

            auto state = inputChannels ? (event & 0xf) : (event & 1);
            auto shift = inputChannels ? 4 : 1;
            GEODE_UNWRAP_INTO(auto tick,
                              checkedTick(previousTick, event >> shift, replay.tickCount, offset));

            auto button = InputButton::Jump;
            auto player = InputPlayer::Player1;
            if (inputChannels) {
                auto buttonValue = (state >> 2) + 1;
                if (buttonValue > static_cast<uint64_t>(InputButton::Right)) {
                    return failure(CodecError::InvalidInput, offset);
                }
                button = static_cast<InputButton>(buttonValue);
                if ((state & 2) != 0)
                    player = InputPlayer::Player2;
            }

            replay.inputs.push_back({.beforeTick = tick,
                                     .button = button,
                                     .player = player,
                                     .pressed = (state & 1) != 0});
            previousTick = tick;
        }

        if ((flags & FrameFixFlag) != 0) {
            GEODE_UNWRAP_INTO(auto schema, reader.readByte());
            if (schema != LegacyFrameFixSchema && schema != PositionFrameFixSchema &&
                schema != FrameFixSchema) {
                return failure(CodecError::UnsupportedFrameFixSchema, reader.position() - 1);
            }

            GEODE_UNWRAP_INTO(auto frameFixSize, reader.readVarint());
            if (frameFixSize > MaximumFrameFixSize) {
                return failure(CodecError::FileTooLarge, reader.position());
            }
            if (frameFixSize > reader.remaining()) {
                return failure(CodecError::Truncated, reader.position());
            }

            GEODE_UNWRAP_INTO(auto frameFixSpan,
                              reader.readSpan(static_cast<size_t>(frameFixSize)));
            Reader frameFixReader(frameFixSpan);

            GEODE_UNWRAP_INTO(auto count, frameFixReader.readVarint());
            if (count > MaximumFrameFixes) {
                return failure(CodecError::TooManyFrameFixes, frameFixReader.position());
            }
            if (count > frameFixReader.remaining() / MinimumFrameFixRecordSize) {
                return failure(CodecError::InvalidFrameFix, frameFixReader.position());
            }

            replay.frameFixes.reserve(static_cast<size_t>(count));
            previousTick = 0;
            for (uint64_t index = 0; index < count; ++index) {
                GEODE_UNWRAP_INTO(auto frameFix,
                                  readFrameFix(frameFixReader,
                                               schema,
                                               previousTick,
                                               replay.tickCount));
                previousTick = frameFix.afterTick;
                replay.frameFixes.push_back(frameFix);
            }

            if (!frameFixReader.finished()) {
                return failure(CodecError::TrailingData, frameFixReader.position());
            }
        }

        if (!reader.finished()) {
            return failure(CodecError::TrailingData, reader.position());
        }

        return Ok(replay);
    }

    std::string_view errorMessage(CodecError error) {
        switch (error) {
        case CodecError::FileTooSmall:
            return "The replay file is too small";
        case CodecError::FileTooLarge:
            return "The replay file is too large";
        case CodecError::ChecksumMismatch:
            return "The replay checksum does not match";
        case CodecError::InvalidMagic:
            return "The file is not a TTRL replay";
        case CodecError::UnsupportedVersion:
            return "The replay version is not supported";
        case CodecError::UnsupportedFlags:
            return "The replay uses unsupported features";
        case CodecError::UnsupportedFrameFixSchema:
            return "The frame fix format is not supported";
        case CodecError::Truncated:
            return "The replay file is incomplete";
        case CodecError::InvalidVarint:
            return "The replay contains an invalid number";
        case CodecError::InvalidTps:
            return "The replay TPS is invalid";
        case CodecError::InvalidGameVersion:
            return "The game version is invalid";
        case CodecError::InvalidInput:
            return "The replay contains an invalid input";
        case CodecError::InvalidInputOrder:
            return "The replay inputs are out of order";
        case CodecError::InvalidFrameFix:
            return "The replay contains an invalid frame fix";
        case CodecError::TooManyInputs:
            return "The replay contains too many inputs";
        case CodecError::TooManyFrameFixes:
            return "The replay contains too many frame fixes";
        case CodecError::TickOverflow:
            return "A replay tick is too large";
        case CodecError::TickOutsideReplay:
            return "A replay tick is outside the replay";
        case CodecError::TrailingData:
            return "The replay contains unexpected trailing data";
        }
        return "The replay is invalid";
    }
} // namespace toasty::replay::ttrl::codec

#include "TtrlCodec.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <numeric>

namespace {
    using toasty::replay::FrameFix;
    using toasty::replay::InputButton;
    using toasty::replay::InputPlayer;
    using toasty::replay::Replay;
    using toasty::replay::ttrl::CodecError;
    using toasty::replay::ttrl::CodecFailure;

    constexpr std::array<uint8_t, 4> Magic = { 'T', 'T', 'R', 'L' };
    constexpr uint8_t Version = 1;
    constexpr uint8_t FrameFixFlag = 1;
    constexpr uint8_t InputChannelsFlag = 2;
    constexpr uint8_t KnownFlags = FrameFixFlag | InputChannelsFlag;
    constexpr uint8_t FrameFixSchema = 1;
    constexpr size_t MinimumFileSize = 25;
    constexpr size_t MaximumFileSize = 64 * 1024 * 1024;
    constexpr size_t MaximumInputSize = 8 * 1024 * 1024;
    constexpr size_t MaximumFrameFixSize = MaximumFileSize - MaximumInputSize;

    CodecFailure failure(CodecError error, size_t offset) {
        return { error, offset };
    }

    bool validInputButton(InputButton button) {
        return
            button == InputButton::Jump ||
            button == InputButton::Left ||
            button == InputButton::Right;
    }

    bool validInputPlayer(InputPlayer player) {
        return player == InputPlayer::Player1 || player == InputPlayer::Player2;
    }

    bool usesInputChannels(Replay const& replay) {
        return std::any_of(
            replay.inputs.begin(),
            replay.inputs.end(),
            [](auto const& event) {
                return
                    event.button != InputButton::Jump ||
                    event.player != InputPlayer::Player1;
            }
        );
    }

    void appendVarint(std::vector<uint8_t>& bytes, uint64_t value) {
        do {
            auto byte = static_cast<uint8_t>(value & 0x7f);
            value >>= 7;
            if (value != 0) byte |= 0x80;
            bytes.push_back(byte);
        } while (value != 0);
    }

    void append32(std::vector<uint8_t>& bytes, uint32_t value) {
        for (uint32_t shift = 0; shift < 32; shift += 8) {
            bytes.push_back(static_cast<uint8_t>(value >> shift));
        }
    }

    void append64(std::vector<uint8_t>& bytes, uint64_t value) {
        for (uint32_t shift = 0; shift < 64; shift += 8) {
            bytes.push_back(static_cast<uint8_t>(value >> shift));
        }
    }

    void appendFloat(std::vector<uint8_t>& bytes, float value) {
        append32(bytes, std::bit_cast<uint32_t>(value));
    }

    void appendDouble(std::vector<uint8_t>& bytes, double value) {
        append64(bytes, std::bit_cast<uint64_t>(value));
    }

    uint32_t checksum(std::span<uint8_t const> bytes) {
        uint32_t value = 0xffffffff;
        for (auto byte : bytes) {
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
        explicit Reader(std::span<uint8_t const> bytes)
          : m_bytes(bytes) {}

        std::expected<uint8_t, CodecFailure> readByte() {
            if (m_position >= m_bytes.size()) {
                return std::unexpected(failure(CodecError::Truncated, m_position));
            }
            return m_bytes[m_position++];
        }

        std::expected<uint64_t, CodecFailure> readVarint() {
            auto start = m_position;
            uint64_t value = 0;

            for (uint32_t shift = 0; shift <= 63; shift += 7) {
                auto result = readByte();
                if (!result) return std::unexpected(result.error());

                auto byte = *result;
                if (shift == 63 && (byte & 0xfe) != 0) {
                    return std::unexpected(failure(CodecError::InvalidVarint, start));
                }

                value |= static_cast<uint64_t>(byte & 0x7f) << shift;
                if ((byte & 0x80) == 0) {
                    if (shift != 0 && byte == 0) {
                        return std::unexpected(failure(CodecError::InvalidVarint, start));
                    }
                    return value;
                }
            }

            return std::unexpected(failure(CodecError::InvalidVarint, start));
        }

        std::expected<uint32_t, CodecFailure> read32() {
            auto result = readSpan(4);
            if (!result) return std::unexpected(result.error());

            auto bytes = *result;
            return
                static_cast<uint32_t>(bytes[0]) |
                (static_cast<uint32_t>(bytes[1]) << 8) |
                (static_cast<uint32_t>(bytes[2]) << 16) |
                (static_cast<uint32_t>(bytes[3]) << 24);
        }

        std::expected<uint64_t, CodecFailure> read64() {
            auto result = readSpan(8);
            if (!result) return std::unexpected(result.error());

            uint64_t value = 0;
            auto bytes = *result;
            for (uint32_t shift = 0; shift < 64; shift += 8) {
                value |= static_cast<uint64_t>(bytes[shift / 8]) << shift;
            }
            return value;
        }

        std::expected<float, CodecFailure> readFloat() {
            auto result = read32();
            if (!result) return std::unexpected(result.error());
            return std::bit_cast<float>(*result);
        }

        std::expected<double, CodecFailure> readDouble() {
            auto result = read64();
            if (!result) return std::unexpected(result.error());
            return std::bit_cast<double>(*result);
        }

        std::expected<std::span<uint8_t const>, CodecFailure> readSpan(size_t size) {
            if (size > m_bytes.size() - m_position) {
                return std::unexpected(failure(CodecError::Truncated, m_position));
            }

            auto result = m_bytes.subspan(m_position, size);
            m_position += size;
            return result;
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
        std::span<uint8_t const> m_bytes;
        size_t m_position = 0;
    };

    std::expected<uint64_t, CodecFailure> checkedTick(
        uint64_t previous,
        uint64_t delta,
        uint64_t duration,
        size_t offset
    ) {
        if (delta > std::numeric_limits<uint64_t>::max() - previous) {
            return std::unexpected(failure(CodecError::TickOverflow, offset));
        }

        auto tick = previous + delta;
        if (tick > duration) {
            return std::unexpected(failure(CodecError::TickPastDuration, offset));
        }
        return tick;
    }

    std::expected<FrameFix, CodecFailure> readFrameFix(
        Reader& reader,
        uint64_t previousTick,
        uint64_t duration
    ) {
        auto offset = reader.position();
        auto delta = reader.readVarint();
        if (!delta) return std::unexpected(delta.error());

        auto tick = checkedTick(previousTick, *delta, duration, offset);
        if (!tick) return std::unexpected(tick.error());

        auto x = reader.readFloat();
        if (!x) return std::unexpected(x.error());
        auto y = reader.readFloat();
        if (!y) return std::unexpected(y.error());
        auto rotation = reader.readFloat();
        if (!rotation) return std::unexpected(rotation.error());
        auto verticalVelocity = reader.readDouble();
        if (!verticalVelocity) return std::unexpected(verticalVelocity.error());

        if (
            !std::isfinite(*x) ||
            !std::isfinite(*y) ||
            !std::isfinite(*rotation) ||
            !std::isfinite(*verticalVelocity)
        ) {
            return std::unexpected(failure(CodecError::InvalidFrameFix, offset));
        }

        return FrameFix {
            .tick = *tick,
            .x = *x,
            .y = *y,
            .rotation = *rotation,
            .verticalVelocity = *verticalVelocity
        };
    }

    std::expected<void, CodecFailure> validate(Replay const& replay, bool inputChannels) {
        if (
            replay.tps.numerator == 0 ||
            replay.tps.denominator == 0 ||
            std::gcd(replay.tps.numerator, replay.tps.denominator) != 1
        ) {
            return std::unexpected(failure(CodecError::InvalidTps, 0));
        }

        if (replay.gameVersion == 0) {
            return std::unexpected(failure(CodecError::InvalidGameVersion, 0));
        }

        uint64_t previousTick = 0;
        for (size_t index = 0; index < replay.inputs.size(); ++index) {
            auto const& event = replay.inputs[index];
            if (!validInputButton(event.button) || !validInputPlayer(event.player)) {
                return std::unexpected(failure(CodecError::InvalidInput, index));
            }
            if (index != 0 && event.tick < previousTick) {
                return std::unexpected(failure(CodecError::InvalidInputOrder, index));
            }
            if (event.tick > replay.durationTicks) {
                return std::unexpected(failure(CodecError::TickPastDuration, index));
            }
            auto shift = inputChannels ? 4 : 1;
            if (event.tick - previousTick > (std::numeric_limits<uint64_t>::max() >> shift)) {
                return std::unexpected(failure(CodecError::TickOverflow, index));
            }
            previousTick = event.tick;
        }

        previousTick = 0;
        for (size_t index = 0; index < replay.frameFixes.size(); ++index) {
            auto const& fix = replay.frameFixes[index];
            if (index != 0 && fix.tick < previousTick) {
                return std::unexpected(failure(CodecError::InvalidFrameFix, index));
            }
            if (
                fix.tick > replay.durationTicks ||
                !std::isfinite(fix.x) ||
                !std::isfinite(fix.y) ||
                !std::isfinite(fix.rotation) ||
                !std::isfinite(fix.verticalVelocity)
            ) {
                return std::unexpected(failure(CodecError::InvalidFrameFix, index));
            }
            previousTick = fix.tick;
        }

        return {};
    }
}

namespace toasty::replay::ttrl {
    EncodeResult encode(Replay const& replay) {
        auto inputChannels = usesInputChannels(replay);
        auto validation = validate(replay, inputChannels);
        if (!validation) return std::unexpected(validation.error());

        std::vector<uint8_t> inputBytes;
        uint64_t previousTick = 0;
        for (auto const& event : replay.inputs) {
            if (inputBytes.size() > MaximumInputSize - 10) {
                return std::unexpected(failure(CodecError::FileTooLarge, inputBytes.size()));
            }
            auto delta = event.tick - previousTick;
            if (inputChannels) {
                auto state =
                    ((static_cast<uint64_t>(event.button) - 1) << 2) |
                    (event.player == InputPlayer::Player2 ? 2 : 0) |
                    static_cast<uint64_t>(event.pressed);
                appendVarint(inputBytes, (delta << 4) | state);
            } else {
                appendVarint(inputBytes, (delta << 1) | static_cast<uint64_t>(event.pressed));
            }
            previousTick = event.tick;
        }

        std::vector<uint8_t> frameFixBytes;
        if (!replay.frameFixes.empty()) {
            appendVarint(frameFixBytes, replay.frameFixes.size());
            previousTick = 0;
            for (auto const& fix : replay.frameFixes) {
                if (frameFixBytes.size() > MaximumFrameFixSize - 30) {
                    return std::unexpected(failure(CodecError::FileTooLarge, frameFixBytes.size()));
                }
                appendVarint(frameFixBytes, fix.tick - previousTick);
                appendFloat(frameFixBytes, fix.x);
                appendFloat(frameFixBytes, fix.y);
                appendFloat(frameFixBytes, fix.rotation);
                appendDouble(frameFixBytes, fix.verticalVelocity);
                previousTick = fix.tick;
            }
        }

        std::vector<uint8_t> bytes;
        bytes.insert(bytes.end(), Magic.begin(), Magic.end());
        bytes.push_back(Version);
        uint8_t flags = 0;
        if (!replay.frameFixes.empty()) flags |= FrameFixFlag;
        if (inputChannels) flags |= InputChannelsFlag;
        bytes.push_back(flags);
        appendVarint(bytes, replay.tps.numerator);
        appendVarint(bytes, replay.tps.denominator);
        appendVarint(bytes, replay.gameVersion);
        appendVarint(bytes, replay.levelId);
        appendVarint(bytes, replay.levelRevision);
        append64(bytes, replay.levelFingerprint);
        appendVarint(bytes, replay.durationTicks);
        appendVarint(bytes, inputBytes.size());
        bytes.insert(bytes.end(), inputBytes.begin(), inputBytes.end());

        if (!frameFixBytes.empty()) {
            bytes.push_back(FrameFixSchema);
            appendVarint(bytes, frameFixBytes.size());
            bytes.insert(bytes.end(), frameFixBytes.begin(), frameFixBytes.end());
        }

        if (bytes.size() > MaximumFileSize - 4) {
            return std::unexpected(failure(CodecError::FileTooLarge, bytes.size()));
        }

        append32(bytes, checksum(bytes));
        return bytes;
    }

    DecodeResult decode(std::span<uint8_t const> bytes) {
        if (bytes.size() < MinimumFileSize) {
            return std::unexpected(failure(CodecError::FileTooSmall, bytes.size()));
        }
        if (bytes.size() > MaximumFileSize) {
            return std::unexpected(failure(CodecError::FileTooLarge, bytes.size()));
        }

        auto payloadSize = bytes.size() - 4;
        auto storedChecksum =
            static_cast<uint32_t>(bytes[payloadSize]) |
            (static_cast<uint32_t>(bytes[payloadSize + 1]) << 8) |
            (static_cast<uint32_t>(bytes[payloadSize + 2]) << 16) |
            (static_cast<uint32_t>(bytes[payloadSize + 3]) << 24);
        if (checksum(bytes.first(payloadSize)) != storedChecksum) {
            return std::unexpected(failure(CodecError::ChecksumMismatch, payloadSize));
        }

        Reader reader(bytes.first(payloadSize));
        auto magic = reader.readSpan(Magic.size());
        if (!magic) return std::unexpected(magic.error());
        if (!std::equal(magic->begin(), magic->end(), Magic.begin())) {
            return std::unexpected(failure(CodecError::InvalidMagic, 0));
        }

        auto version = reader.readByte();
        if (!version) return std::unexpected(version.error());
        if (*version != Version) {
            return std::unexpected(failure(CodecError::UnsupportedVersion, reader.position() - 1));
        }

        auto flags = reader.readByte();
        if (!flags) return std::unexpected(flags.error());
        if ((*flags & ~KnownFlags) != 0) {
            return std::unexpected(failure(CodecError::UnsupportedFlags, reader.position() - 1));
        }
        auto inputChannels = (*flags & InputChannelsFlag) != 0;

        Replay replay;

        auto numerator = reader.readVarint();
        if (!numerator) return std::unexpected(numerator.error());
        auto denominator = reader.readVarint();
        if (!denominator) return std::unexpected(denominator.error());
        if (
            *numerator == 0 ||
            *denominator == 0 ||
            std::gcd(*numerator, *denominator) != 1
        ) {
            return std::unexpected(failure(CodecError::InvalidTps, reader.position()));
        }
        replay.tps = { *numerator, *denominator };

        auto gameVersion = reader.readVarint();
        if (!gameVersion) return std::unexpected(gameVersion.error());
        if (*gameVersion == 0 || *gameVersion > std::numeric_limits<uint32_t>::max()) {
            return std::unexpected(failure(CodecError::InvalidGameVersion, reader.position()));
        }
        replay.gameVersion = static_cast<uint32_t>(*gameVersion);

        auto levelId = reader.readVarint();
        if (!levelId) return std::unexpected(levelId.error());
        replay.levelId = *levelId;

        auto levelRevision = reader.readVarint();
        if (!levelRevision) return std::unexpected(levelRevision.error());
        replay.levelRevision = *levelRevision;

        auto levelFingerprint = reader.read64();
        if (!levelFingerprint) return std::unexpected(levelFingerprint.error());
        replay.levelFingerprint = *levelFingerprint;

        auto duration = reader.readVarint();
        if (!duration) return std::unexpected(duration.error());
        replay.durationTicks = *duration;

        auto inputSize = reader.readVarint();
        if (!inputSize) return std::unexpected(inputSize.error());
        if (*inputSize > MaximumInputSize) {
            return std::unexpected(failure(CodecError::FileTooLarge, reader.position()));
        }
        if (*inputSize > reader.remaining()) {
            return std::unexpected(failure(CodecError::Truncated, reader.position()));
        }

        auto inputSpan = reader.readSpan(static_cast<size_t>(*inputSize));
        if (!inputSpan) return std::unexpected(inputSpan.error());
        Reader inputReader(*inputSpan);

        uint64_t previousTick = 0;
        while (!inputReader.finished()) {
            auto offset = inputReader.position();
            auto event = inputReader.readVarint();
            if (!event) return std::unexpected(event.error());

            auto state = inputChannels ? (*event & 0xf) : (*event & 1);
            auto shift = inputChannels ? 4 : 1;
            auto tick = checkedTick(previousTick, *event >> shift, replay.durationTicks, offset);
            if (!tick) return std::unexpected(tick.error());

            auto button = InputButton::Jump;
            auto player = InputPlayer::Player1;
            if (inputChannels) {
                auto buttonValue = (state >> 2) + 1;
                if (buttonValue > static_cast<uint64_t>(InputButton::Right)) {
                    return std::unexpected(failure(CodecError::InvalidInput, offset));
                }
                button = static_cast<InputButton>(buttonValue);
                if ((state & 2) != 0) player = InputPlayer::Player2;
            }

            replay.inputs.push_back({
                .tick = *tick,
                .button = button,
                .player = player,
                .pressed = (state & 1) != 0
            });
            previousTick = *tick;
        }

        if ((*flags & FrameFixFlag) != 0) {
            auto schema = reader.readByte();
            if (!schema) return std::unexpected(schema.error());
            if (*schema != FrameFixSchema) {
                return std::unexpected(
                    failure(CodecError::UnsupportedFrameFixSchema, reader.position() - 1)
                );
            }

            auto frameFixSize = reader.readVarint();
            if (!frameFixSize) return std::unexpected(frameFixSize.error());
            if (*frameFixSize > MaximumFrameFixSize) {
                return std::unexpected(failure(CodecError::FileTooLarge, reader.position()));
            }
            if (*frameFixSize > reader.remaining()) {
                return std::unexpected(failure(CodecError::Truncated, reader.position()));
            }

            auto frameFixSpan = reader.readSpan(static_cast<size_t>(*frameFixSize));
            if (!frameFixSpan) return std::unexpected(frameFixSpan.error());
            Reader frameFixReader(*frameFixSpan);

            auto count = frameFixReader.readVarint();
            if (!count) return std::unexpected(count.error());
            if (*count > frameFixReader.remaining()) {
                return std::unexpected(
                    failure(CodecError::InvalidFrameFix, frameFixReader.position())
                );
            }

            replay.frameFixes.reserve(static_cast<size_t>(*count));
            previousTick = 0;
            for (uint64_t index = 0; index < *count; ++index) {
                auto frameFix = readFrameFix(
                    frameFixReader,
                    previousTick,
                    replay.durationTicks
                );
                if (!frameFix) return std::unexpected(frameFix.error());
                previousTick = frameFix->tick;
                replay.frameFixes.push_back(*frameFix);
            }

            if (!frameFixReader.finished()) {
                return std::unexpected(
                    failure(CodecError::TrailingData, frameFixReader.position())
                );
            }
        }

        if (!reader.finished()) {
            return std::unexpected(failure(CodecError::TrailingData, reader.position()));
        }

        return replay;
    }
}

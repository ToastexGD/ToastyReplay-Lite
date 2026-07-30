#include "TtrlStorage.hpp"

#include <asp/fs.hpp>
#include <asp/iter.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/utils/string.hpp>
#include <Geode/utils/StringBuffer.hpp>

#include <algorithm>
#include <utility>

using namespace geode::prelude;

namespace {
    using toasty::replay::ttrl::CodecFailure;
    using toasty::replay::ttrl::StorageError;
    using toasty::replay::ttrl::StorageFailure;

    constexpr size_t MaximumReplayFiles = 4096;
    constexpr size_t MaximumReplayName = 80;
    constexpr size_t MaximumNameAttempts = 10000;

    impl::ErrContainer<StorageFailure> failure(StorageError error,
                                               std::string detail = {},
                                               std::optional<CodecFailure> codec = std::nullopt) {
        return Err(StorageFailure{error, std::move(detail), codec});
    }

    bool endsWithTtrl(std::string_view value) {
        if (value.size() < 5)
            return false;
        return utils::string::equalsIgnoreCase(value.substr(value.size() - 5), ".ttrl");
    }

    bool reservedName(std::string_view value) {
        auto lower = utils::string::toLower(std::string(value));

        if (lower == "con" || lower == "prn" || lower == "aux" || lower == "nul" ||
            lower == "clock$") {
            return true;
        }
        if (lower.size() != 4)
            return false;
        if (lower[3] < '1' || lower[3] > '9')
            return false;
        return lower.starts_with("com") || lower.starts_with("lpt");
    }

    std::string replayBaseName(std::string_view input) {
        if (endsWithTtrl(input))
            input.remove_suffix(5);

        StringBuffer<MaximumReplayName> buffer;
        bool previousSpace = false;

        for (auto byte : input) {
            if (buffer.size() == MaximumReplayName)
                break;

            auto value = static_cast<unsigned char>(byte);
            if (value == ' ' || value == '\t') {
                if (buffer.size() > 0 && !previousSpace)
                    buffer.append(' ');
                previousSpace = true;
                continue;
            }

            previousSpace = false;
            if (std::isalnum(value) || value == '-' || value == '_' || value == '.' ||
                value == '(' || value == ')' || value == '[' || value == ']') {
                buffer.append(static_cast<char>(value));
            } else {
                buffer.append('_');
            }
        }

        auto str = buffer.str();
        utils::string::trimIP(str, " .");
        if (str.empty())
            str = "Replay";
        if (reservedName(str))
            str.push_back('_');
        return str;
    }

    std::string replayFileName(std::string_view name, size_t index) {
        StringBuffer<256> buf;
        auto base = replayBaseName(name);
        if (index != 0) {
            buf.append("{} ({})", base, index + 1);
        } else {
            buf.append("{}", base);
        }
        buf.append(".ttrl");
        return buf.str();
    }

    Result<void, StorageFailure> ensureDirectory(asp::fs::path const& directory) {
        if (asp::fs::exists(directory)) {
            auto isDirRes = asp::fs::isDirectory(directory);
            if (isDirRes.isOk() && isDirRes.unwrap())
                return Ok();
            return failure(StorageError::DirectoryUnavailable);
        }

        auto createRes = utils::file::createDirectoryAll(directory);
        if (createRes.isErr()) {
            return failure(StorageError::DirectoryUnavailable, createRes.unwrapErr());
        }
        return Ok();
    }

    Result<asp::fs::path, StorageFailure> availablePath(asp::fs::path const& directory,
                                                        std::string_view name) {
        for (size_t index = 0; index < MaximumNameAttempts; ++index) {
            auto path = directory / replayFileName(name, index);
            if (!asp::fs::exists(path))
                return Ok(path);
        }
        return failure(StorageError::TooManyFiles);
    }

    Result<asp::fs::path, StorageFailure> temporaryPath(asp::fs::path const& target) {
        for (size_t index = 0; index < MaximumNameAttempts; ++index) {
            auto path = target;
            path += index == 0 ? ".tmp" : ".tmp" + std::to_string(index + 1);
            if (!asp::fs::exists(path))
                return Ok(path);
        }
        return failure(StorageError::TooManyFiles);
    }

    asp::fs::Result<void> removeTemporary(asp::fs::path const& path) {
        auto res = asp::fs::remove(path);
        if (res.isErr()) {
            log::warn("Failed to remove temporary replay file at {}: {}",
                      path,
                      res.unwrapErr().message());
        }
        return res;
    }

    std::string pathFileName(asp::fs::path const& path) {
        return utils::string::pathToString(path.filename());
    }
} // namespace

namespace toasty::replay::ttrl {
    Storage::Storage(asp::fs::path directory)
        : m_directory(std::move(directory)) {}

    asp::fs::path const& Storage::directory() const {
        return m_directory;
    }

    SaveResult Storage::save(ZStringView name, Replay const& replay) const {
        GEODE_UNWRAP(ensureDirectory(m_directory));

        auto encoded = encode(replay);
        if (encoded.isErr()) {
            return failure(StorageError::InvalidReplay, {}, encoded.unwrapErr());
        }

        GEODE_UNWRAP_INTO(auto target, availablePath(m_directory, name));
        GEODE_UNWRAP_INTO(auto temporary, temporaryPath(target));

        auto writeRes = utils::file::writeBinary(
            temporary, ByteSpan(encoded.unwrap().data(), encoded.unwrap().size()));
        if (writeRes.isErr()) {
            static_cast<void>(removeTemporary(temporary));
            return failure(StorageError::WriteFailed, writeRes.unwrapErr());
        }

        auto renameRes = asp::fs::rename(temporary, target);
        if (renameRes.isErr()) {
            static_cast<void>(removeTemporary(temporary));
            return failure(StorageError::RenameFailed);
        }

        return Ok(pathFileName(target));
    }

    LoadResult Storage::load(ZStringView fileName) const {
        GEODE_UNWRAP(ensureDirectory(m_directory));

        auto path = m_directory / replayFileName(fileName, 0);
        if (!asp::fs::exists(path)) {
            return failure(StorageError::FileNotFound);
        }

        auto isFileRes = asp::fs::isFile(path);
        if (isFileRes.isErr() || !isFileRes.unwrap()) {
            return failure(StorageError::InvalidFile);
        }

        auto readRes = utils::file::readBinary(path);
        if (readRes.isErr()) {
            return failure(StorageError::ReadFailed, readRes.unwrapErr());
        }

        auto bytes = std::move(readRes.unwrap());
        if (bytes.size() > MaximumFileSize) {
            return failure(StorageError::FileTooLarge);
        }

        auto replay = decode(bytes);
        if (replay.isErr()) {
            return failure(StorageError::InvalidReplay, {}, replay.unwrapErr());
        }
        return Ok(std::move(replay.unwrap()));
    }

    ListResult Storage::list() const {
        GEODE_UNWRAP(ensureDirectory(m_directory));

        auto dirRes = utils::file::readDirectory(m_directory);
        if (dirRes.isErr()) {
            return failure(StorageError::DirectoryUnavailable, dirRes.unwrapErr());
        }

        std::vector<std::string> files;
        for (auto const& path : asp::iter::from(dirRes.unwrap())) {
            auto isFileRes = asp::fs::isFile(path);
            if (isFileRes.isOk() && isFileRes.unwrap()) {
                auto name = pathFileName(path);
                if (endsWithTtrl(name)) {
                    if (files.size() == MaximumReplayFiles) {
                        return failure(StorageError::TooManyFiles);
                    }
                    files.push_back(std::move(name));
                }
            }
        }

        std::sort(files.begin(), files.end());
        return Ok(files);
    }

    std::string_view errorMessage(StorageError error) {
        switch (error) {
        case StorageError::DirectoryUnavailable:
            return "The replay directory is unavailable";
        case StorageError::TooManyFiles:
            return "There are too many replay files";
        case StorageError::FileNotFound:
            return "The replay file was not found";
        case StorageError::InvalidFile:
            return "The replay path is not a regular file";
        case StorageError::FileTooLarge:
            return "The replay file is too large";
        case StorageError::OpenFailed:
            return "The replay file could not be opened";
        case StorageError::ReadFailed:
            return "The replay file could not be read";
        case StorageError::WriteFailed:
            return "The replay file could not be written";
        case StorageError::RenameFailed:
            return "The replay file could not be finalized";
        case StorageError::InvalidReplay:
            return "The replay data is invalid";
        }
        return "The replay operation failed";
    }

    std::string describe(StorageFailure const& failure) {
        std::string msg = fmt::format("{}", errorMessage(failure.error));
        if (failure.codec) {
            fmt::format_to(std::back_inserter(msg), ": {}", errorMessage(failure.codec->error));
        }
        if (!failure.detail.empty()) {
            fmt::format_to(std::back_inserter(msg), ": {}", failure.detail);
        }
        return msg;
    }
} // namespace toasty::replay::ttrl

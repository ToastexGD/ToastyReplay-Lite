#include "TtrlStorage.hpp"

#include <asp/fs.hpp>
#include <asp/iter.hpp>
#include <Geode/utils/string.hpp>
#include <Geode/utils/StringBuffer.hpp>

#include <algorithm>
#include <cctype>
#include <utility>

using namespace geode::prelude;

namespace toasty::replay::ttrl {
    constexpr size_t MaximumReplayFiles = 4096;
    constexpr size_t MaximumReplayName = 80;
    constexpr size_t MaximumNameAttempts = 10000;

    static auto failure(StorageError error,
                        std::string detail = {},
                        std::optional<codec::CodecFailure> codec = std::nullopt) {
        return Err(StorageFailure{error, std::move(detail), codec});
    }

    static bool endsWithTtrl(ZStringView value) {
        auto view = value.view();
        if (view.size() < 5)
            return false;
        return utils::string::equalsIgnoreCase(view.substr(view.size() - 5), ".ttrl");
    }

    static bool reservedName(ZStringView value) {
        auto lower = utils::string::toLower(std::string(value.view()));
        auto device = ZStringView(lower).view().substr(0, lower.find('.'));

        if (device == "con" || device == "prn" || device == "aux" || device == "nul" ||
            device == "clock$") {
            return true;
        }
        if (device.size() != 4)
            return false;
        if (device[3] < '1' || device[3] > '9')
            return false;
        return device.starts_with("com") || device.starts_with("lpt");
    }

    static std::string replayBaseName(ZStringView input) {
        auto view = input.view();
        if (endsWithTtrl(input))
            view.remove_suffix(5);

        StringBuffer<MaximumReplayName> buffer;
        bool previousSpace = false;

        for (auto byte : asp::iter::from(view)) {
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

    static std::string replayFileName(ZStringView name, size_t index) {
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

    static std::string replayLoadFileName(ZStringView name) {
        auto view = name.view();
        auto extension = endsWithTtrl(name) ? view.substr(view.size() - 5) : std::string_view(".ttrl");
        StringBuffer<256> buf;
        buf.append("{}{}", replayBaseName(name), extension);
        return buf.str();
    }

    static Result<void, StorageFailure> ensureDirectory(asp::fs::path const& directory) {
        auto statusRes = asp::fs::status(directory);
        if (statusRes.isOk()) {
            if (statusRes.unwrap().isDirectory())
                return Ok();
            return failure(StorageError::DirectoryUnavailable);
        }

        auto statusError = statusRes.unwrapErr();
        if (statusError.getCode() != std::errc::no_such_file_or_directory) {
            return failure(StorageError::DirectoryUnavailable, statusError.message());
        }

        auto createRes = asp::fs::createDirAll(directory);
        if (createRes.isErr()) {
            return failure(StorageError::DirectoryUnavailable, createRes.unwrapErr().message());
        }
        return Ok();
    }

    static Result<asp::fs::path, StorageFailure> availablePath(asp::fs::path const& directory,
                                                               ZStringView name) {
        for (size_t index = 0; index < MaximumNameAttempts; ++index) {
            auto path = directory / replayFileName(name, index);
            auto statusRes = asp::fs::status(path);
            if (statusRes.isErr()) {
                if (statusRes.unwrapErr().getCode() == std::errc::no_such_file_or_directory) {
                    return Ok(path);
                }
                return failure(StorageError::DirectoryUnavailable, statusRes.unwrapErr().message());
            }
        }
        return failure(StorageError::TooManyFiles);
    }

    static Result<asp::fs::path, StorageFailure> temporaryPath(asp::fs::path const& target) {
        auto targetStr = utils::string::pathToString(target);
        for (size_t index = 0; index < MaximumNameAttempts; ++index) {
            StringBuffer<256> buf;
            if (index == 0) {
                buf.append("{}.tmp", targetStr);
            } else {
                buf.append("{}.tmp{}", targetStr, index + 1);
            }
            asp::fs::path path(buf.str());
            auto statusRes = asp::fs::status(path);
            if (statusRes.isErr()) {
                if (statusRes.unwrapErr().getCode() == std::errc::no_such_file_or_directory) {
                    return Ok(path);
                }
                return failure(StorageError::DirectoryUnavailable, statusRes.unwrapErr().message());
            }
        }
        return failure(StorageError::TooManyFiles);
    }

    static asp::fs::Result<void> removeTemporary(asp::fs::path const& path) {
        auto res = asp::fs::remove(path);
        if (res.isErr()) {
            log::warn("Failed to remove temporary replay file at {}: {}", path,
                      res.unwrapErr().message());
        }
        return res;
    }

    static std::string pathFileName(asp::fs::path const& path) {
        return utils::string::pathToString(path.filename());
    }

    Storage::Storage(asp::fs::path directory)
        : m_directory(std::move(directory)) {}

    asp::fs::path const& Storage::directory() const {
        return m_directory;
    }

    SaveResult Storage::save(ZStringView name, Replay const& replay) const {
        GEODE_UNWRAP(ensureDirectory(m_directory));

        auto encoded = codec::encode(replay);
        if (encoded.isErr()) {
            return failure(StorageError::InvalidReplay, {}, encoded.unwrapErr());
        }

        GEODE_UNWRAP_INTO(auto target, availablePath(m_directory, name));
        GEODE_UNWRAP_INTO(auto temporary, temporaryPath(target));

        auto writeRes = asp::fs::write(temporary, encoded.unwrap());
        if (writeRes.isErr()) {
            static_cast<void>(removeTemporary(temporary));
            return failure(StorageError::WriteFailed, writeRes.unwrapErr());
        }

        auto renameRes = asp::fs::rename(temporary, target);
        if (renameRes.isErr()) {
            static_cast<void>(removeTemporary(temporary));
            return failure(StorageError::RenameFailed, renameRes.unwrapErr().message());
        }

        return Ok(pathFileName(target));
    }

    LoadResult Storage::load(ZStringView fileName) const {
        GEODE_UNWRAP(ensureDirectory(m_directory));

        auto path = m_directory / replayLoadFileName(fileName);
        auto statusRes = asp::fs::status(path);
        if (statusRes.isErr()) {
            auto err = statusRes.unwrapErr();
            if (err.getCode() == std::errc::no_such_file_or_directory) {
                return failure(StorageError::FileNotFound);
            }
            return failure(StorageError::ReadFailed, err.message());
        }

        auto status = statusRes.unwrap();
        if (status.isSymlink() || !status.isFile()) {
            return failure(StorageError::InvalidFile);
        }

        auto readRes = asp::fs::read(path);
        if (readRes.isErr()) {
            return failure(StorageError::ReadFailed, readRes.unwrapErr());
        }

        auto bytes = std::move(readRes.unwrap());
        if (bytes.size() > codec::MaximumFileSize) {
            return failure(StorageError::FileTooLarge);
        }

        auto replay = codec::decode(bytes);
        if (replay.isErr()) {
            return failure(StorageError::InvalidReplay, {}, replay.unwrapErr());
        }
        return Ok(std::move(replay.unwrap()));
    }

    ListResult Storage::list() const {
        GEODE_UNWRAP(ensureDirectory(m_directory));

        auto dirRes = asp::fs::iterdir(m_directory);
        if (dirRes.isErr()) {
            return failure(StorageError::DirectoryUnavailable, dirRes.unwrapErr().message());
        }

        std::vector<std::string> files;
        for (auto const& entry : asp::iter::from(dirRes.unwrap())) {
            auto const& path = entry.get().path();
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
        return Ok(std::move(files));
    }

    ZStringView errorMessage(StorageError error) {
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
        StringBuffer<256> msg;
        msg.append("{}", errorMessage(failure.error));
        if (failure.codec) {
            msg.append(": {}", codec::errorMessage(failure.codec->error));
        }
        if (!failure.detail.empty()) {
            msg.append(": {}", failure.detail);
        }
        return msg.str();
    }
} // namespace toasty::replay::ttrl

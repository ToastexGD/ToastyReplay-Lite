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
    constexpr size_t MaximumNameAttempts = 10000;

    static auto failure(StorageError error,
                        std::string detail = {},
                        std::optional<codec::CodecFailure> codec = std::nullopt) {
        return Err(StorageFailure{error, std::move(detail), codec});
    }

    static char asciiLower(char value) {
        auto byte = static_cast<unsigned char>(value);
        if (byte >= 'A' && byte <= 'Z')
            return static_cast<char>(byte - 'A' + 'a');
        return static_cast<char>(byte);
    }

    static bool endsWithTtrl(ZStringView value) {
        auto view = value.view();
        if (view.size() < 5)
            return false;

        constexpr std::string_view extension = ".ttrl";
        auto suffix = view.substr(view.size() - extension.size());
        for (size_t index = 0; index < extension.size(); ++index) {
            if (asciiLower(suffix[index]) != extension[index])
                return false;
        }
        return true;
    }

    static bool reservedName(ZStringView value) {
        std::string lower(value.view());
        for (auto& character : lower) {
            character = asciiLower(character);
        }
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

    static std::string pathFileName(asp::fs::path const& path) {
        return utils::string::pathToString(path.filename());
    }

    static bool validReplayFileName(ZStringView fileName) {
        auto view = fileName.view();
        if (!endsWithTtrl(fileName) || view.find('/') != std::string_view::npos ||
            view.find('\\') != std::string_view::npos || view.find(':') != std::string_view::npos) {
            return false;
        }
        return replayLoadFileName(fileName) == std::string(view);
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
        auto writeRes = utils::file::writeBinarySafe(target, encoded.unwrap());
        if (writeRes.isErr()) {
            return failure(StorageError::WriteFailed, writeRes.unwrapErr());
        }

        return Ok(pathFileName(target));
    }

    SaveResult Storage::importFile(asp::fs::path const& source) const {
        GEODE_UNWRAP(ensureDirectory(m_directory));

        auto statusRes = asp::fs::status(source);
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

        auto readRes = utils::file::readBinary(source);
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

        GEODE_UNWRAP_INTO(auto target, availablePath(m_directory, pathFileName(source)));
        auto writeRes = utils::file::writeBinarySafe(target, bytes);
        if (writeRes.isErr()) {
            return failure(StorageError::WriteFailed, writeRes.unwrapErr());
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

        auto readRes = utils::file::readBinary(path);
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

    RemoveResult Storage::remove(ZStringView fileName) const {
        if (!validReplayFileName(fileName)) {
            return failure(StorageError::InvalidFile);
        }
        GEODE_UNWRAP(ensureDirectory(m_directory));

        auto path = m_directory / std::string(fileName.view());
        auto statusRes = asp::fs::status(path);
        if (statusRes.isErr()) {
            auto err = statusRes.unwrapErr();
            if (err.getCode() == std::errc::no_such_file_or_directory) {
                return failure(StorageError::FileNotFound);
            }
            return failure(StorageError::DeleteFailed, err.message());
        }

        auto status = statusRes.unwrap();
        if (status.isSymlink() || !status.isFile()) {
            return failure(StorageError::InvalidFile);
        }

        auto removeRes = asp::fs::remove(path);
        if (removeRes.isErr()) {
            return failure(StorageError::DeleteFailed, removeRes.unwrapErr().message());
        }
        return Ok();
    }

    RenameResult Storage::rename(ZStringView fileName, ZStringView name) const {
        if (!validReplayFileName(fileName)) {
            return failure(StorageError::InvalidFile);
        }
        GEODE_UNWRAP(ensureDirectory(m_directory));

        auto source = m_directory / std::string(fileName.view());
        auto sourceStatus = asp::fs::status(source);
        if (sourceStatus.isErr()) {
            auto err = sourceStatus.unwrapErr();
            if (err.getCode() == std::errc::no_such_file_or_directory) {
                return failure(StorageError::FileNotFound);
            }
            return failure(StorageError::RenameFailed, err.message());
        }
        auto status = sourceStatus.unwrap();
        if (status.isSymlink() || !status.isFile()) {
            return failure(StorageError::InvalidFile);
        }

        auto targetName = replayFileName(name, 0);
        if (targetName == std::string(fileName.view())) {
            return Ok(std::move(targetName));
        }

        auto target = m_directory / targetName;
        auto targetStatus = asp::fs::status(target);
        if (targetStatus.isOk()) {
            return failure(StorageError::NameTaken);
        }
        if (targetStatus.unwrapErr().getCode() != std::errc::no_such_file_or_directory) {
            return failure(StorageError::RenameFailed, targetStatus.unwrapErr().message());
        }

        auto renameResult = asp::fs::rename(source, target);
        if (renameResult.isErr()) {
            return failure(StorageError::RenameFailed, renameResult.unwrapErr().message());
        }
        return Ok(std::move(targetName));
    }

    ListResult Storage::list() const {
        GEODE_UNWRAP(ensureDirectory(m_directory));

        auto dirRes = asp::fs::iterdir(m_directory);
        if (dirRes.isErr()) {
            return failure(StorageError::DirectoryUnavailable, dirRes.unwrapErr().message());
        }

        std::vector<std::pair<std::filesystem::file_time_type, std::string>> files;
        for (auto const& entry : asp::iter::consume(dirRes.unwrap())) {
            auto const& path = entry.path();
            auto isFileRes = asp::fs::isFile(path);
            if (isFileRes.isOk() && isFileRes.unwrap()) {
                auto name = pathFileName(path);
                if (endsWithTtrl(name)) {
                    if (files.size() == MaximumReplayFiles) {
                        return failure(StorageError::TooManyFiles);
                    }
                    auto writeTime = asp::fs::lastWriteTime(path);
                    files.emplace_back(writeTime.isOk() ? writeTime.unwrap()
                                                        : std::filesystem::file_time_type::min(),
                                       std::move(name));
                }
            }
        }

        std::sort(files.begin(), files.end(), [](auto const& left, auto const& right) {
            if (left.first != right.first) {
                return left.first > right.first;
            }
            return left.second < right.second;
        });
        std::vector<std::string> names;
        names.reserve(files.size());
        for (auto& file : files) {
            names.push_back(std::move(file.second));
        }
        return Ok(std::move(names));
    }

    std::string displayName(ZStringView fileName) {
        auto view = fileName.view();
        if (endsWithTtrl(fileName)) {
            view.remove_suffix(5);
        }
        return std::string(view);
    }

    ZStringView errorMessage(StorageError error) {
        switch (error) {
        case StorageError::DirectoryUnavailable:
            return "The replay directory is unavailable";
        case StorageError::TooManyFiles:
            return "There are too many replay files";
        case StorageError::FileNotFound:
            return "The replay file was not found";
        case StorageError::NameTaken:
            return "A replay already uses that name";
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
        case StorageError::DeleteFailed:
            return "The replay file could not be deleted";
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

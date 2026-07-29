#include "TtrlStorage.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <utility>

namespace {
    using toasty::replay::ttrl::CodecFailure;
    using toasty::replay::ttrl::StorageError;
    using toasty::replay::ttrl::StorageFailure;

    constexpr size_t MaximumReplayFiles = 4096;
    constexpr size_t MaximumReplayName = 80;
    constexpr size_t MaximumNameAttempts = 10000;

    StorageFailure failure(
        StorageError error,
        std::string detail = {},
        std::optional<CodecFailure> codec = std::nullopt
    ) {
        return { error, std::move(detail), codec };
    }

    bool endsWithTtrl(std::string_view value) {
        if (value.size() < 5) return false;
        auto extension = value.substr(value.size() - 5);
        return
            extension[0] == '.' &&
            std::tolower(static_cast<unsigned char>(extension[1])) == 't' &&
            std::tolower(static_cast<unsigned char>(extension[2])) == 't' &&
            std::tolower(static_cast<unsigned char>(extension[3])) == 'r' &&
            std::tolower(static_cast<unsigned char>(extension[4])) == 'l';
    }

    bool reservedName(std::string const& value) {
        std::string lower;
        lower.reserve(value.size());
        for (auto byte : value) {
            lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(byte))));
        }

        if (
            lower == "con" ||
            lower == "prn" ||
            lower == "aux" ||
            lower == "nul" ||
            lower == "clock$"
        ) {
            return true;
        }
        if (lower.size() != 4) return false;
        if (lower[3] < '1' || lower[3] > '9') return false;
        return lower.starts_with("com") || lower.starts_with("lpt");
    }

    std::string replayBaseName(std::string_view input) {
        if (endsWithTtrl(input)) input.remove_suffix(5);

        std::string result;
        result.reserve(std::min(input.size(), MaximumReplayName));
        bool previousSpace = false;

        for (auto byte : input) {
            if (result.size() == MaximumReplayName) break;

            auto value = static_cast<unsigned char>(byte);
            if (value == ' ' || value == '\t') {
                if (!result.empty() && !previousSpace) result.push_back(' ');
                previousSpace = true;
                continue;
            }

            previousSpace = false;
            if (
                std::isalnum(value) ||
                value == '-' ||
                value == '_' ||
                value == '.' ||
                value == '(' ||
                value == ')' ||
                value == '[' ||
                value == ']'
            ) {
                result.push_back(static_cast<char>(value));
            } else {
                result.push_back('_');
            }
        }

        while (!result.empty() && (result.front() == ' ' || result.front() == '.')) {
            result.erase(result.begin());
        }
        while (!result.empty() && (result.back() == ' ' || result.back() == '.')) {
            result.pop_back();
        }
        if (result.empty()) result = "Replay";
        if (reservedName(result)) result.push_back('_');
        return result;
    }

    std::string replayFileName(std::string_view name, size_t index) {
        auto base = replayBaseName(name);
        if (index != 0) {
            base += " (";
            base += std::to_string(index + 1);
            base += ')';
        }
        base += ".ttrl";
        return base;
    }

    std::expected<void, StorageFailure> ensureDirectory(
        std::filesystem::path const& directory
    ) {
        std::error_code error;
        auto status = std::filesystem::status(directory, error);
        if (!error && std::filesystem::exists(status)) {
            if (std::filesystem::is_directory(status)) return {};
            return std::unexpected(failure(StorageError::DirectoryUnavailable));
        }
        if (error && error != std::errc::no_such_file_or_directory) {
            return std::unexpected(
                failure(StorageError::DirectoryUnavailable, error.message())
            );
        }

        error.clear();
        std::filesystem::create_directories(directory, error);
        if (error) {
            return std::unexpected(
                failure(StorageError::DirectoryUnavailable, error.message())
            );
        }
        return {};
    }

    std::expected<std::filesystem::path, StorageFailure> availablePath(
        std::filesystem::path const& directory,
        std::string_view name
    ) {
        for (size_t index = 0; index < MaximumNameAttempts; ++index) {
            auto path = directory / replayFileName(name, index);
            std::error_code error;
            auto exists = std::filesystem::exists(path, error);
            if (error) {
                return std::unexpected(failure(StorageError::DirectoryUnavailable, error.message()));
            }
            if (!exists) return path;
        }
        return std::unexpected(failure(StorageError::TooManyFiles));
    }

    std::expected<std::filesystem::path, StorageFailure> temporaryPath(
        std::filesystem::path const& target
    ) {
        for (size_t index = 0; index < MaximumNameAttempts; ++index) {
            auto path = target;
            path += index == 0 ? ".tmp" : ".tmp" + std::to_string(index + 1);
            std::error_code error;
            auto exists = std::filesystem::exists(path, error);
            if (error) {
                return std::unexpected(failure(StorageError::DirectoryUnavailable, error.message()));
            }
            if (!exists) return path;
        }
        return std::unexpected(failure(StorageError::TooManyFiles));
    }

    void removeTemporary(std::filesystem::path const& path) {
        std::error_code error;
        std::filesystem::remove(path, error);
    }

    std::string pathFileName(std::filesystem::path const& path) {
        auto value = path.filename().u8string();
        return { value.begin(), value.end() };
    }
}

namespace toasty::replay::ttrl {
    Storage::Storage(std::filesystem::path directory)
      : m_directory(std::move(directory)) {}

    std::filesystem::path const& Storage::directory() const {
        return m_directory;
    }

    SaveResult Storage::save(std::string_view name, Replay const& replay) const {
        auto directoryResult = ensureDirectory(m_directory);
        if (!directoryResult) return std::unexpected(directoryResult.error());

        auto encoded = encode(replay);
        if (!encoded) {
            return std::unexpected(
                failure(StorageError::InvalidReplay, {}, encoded.error())
            );
        }

        auto target = availablePath(m_directory, name);
        if (!target) return std::unexpected(target.error());
        auto temporary = temporaryPath(*target);
        if (!temporary) return std::unexpected(temporary.error());

        std::ofstream stream(*temporary, std::ios::binary | std::ios::trunc);
        if (!stream) {
            removeTemporary(*temporary);
            return std::unexpected(failure(StorageError::OpenFailed));
        }

        stream.write(
            reinterpret_cast<char const*>(encoded->data()),
            static_cast<std::streamsize>(encoded->size())
        );
        stream.flush();
        stream.close();
        if (!stream) {
            removeTemporary(*temporary);
            return std::unexpected(failure(StorageError::WriteFailed));
        }

        std::error_code error;
        std::filesystem::rename(*temporary, *target, error);
        if (error) {
            removeTemporary(*temporary);
            return std::unexpected(failure(StorageError::RenameFailed, error.message()));
        }

        return pathFileName(*target);
    }

    LoadResult Storage::load(std::string_view fileName) const {
        auto directoryResult = ensureDirectory(m_directory);
        if (!directoryResult) return std::unexpected(directoryResult.error());

        auto path = m_directory / replayFileName(fileName, 0);
        std::error_code error;
        auto status = std::filesystem::symlink_status(path, error);
        if (error == std::errc::no_such_file_or_directory) {
            return std::unexpected(failure(StorageError::FileNotFound));
        }
        if (error) {
            return std::unexpected(failure(StorageError::ReadFailed, error.message()));
        }
        if (std::filesystem::is_symlink(status) || !std::filesystem::is_regular_file(status)) {
            return std::unexpected(failure(StorageError::InvalidFile));
        }

        auto size = std::filesystem::file_size(path, error);
        if (error) {
            return std::unexpected(failure(StorageError::ReadFailed, error.message()));
        }
        if (size > MaximumFileSize) {
            return std::unexpected(failure(StorageError::FileTooLarge));
        }

        std::ifstream stream(path, std::ios::binary);
        if (!stream) return std::unexpected(failure(StorageError::OpenFailed));

        std::vector<uint8_t> bytes(static_cast<size_t>(size));
        if (!bytes.empty()) {
            stream.read(
                reinterpret_cast<char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size())
            );
            if (stream.gcount() != static_cast<std::streamsize>(bytes.size())) {
                return std::unexpected(failure(StorageError::ReadFailed));
            }
        }
        if (stream.peek() != std::char_traits<char>::eof()) {
            return std::unexpected(failure(StorageError::ReadFailed));
        }

        auto replay = decode(bytes);
        if (!replay) {
            return std::unexpected(
                failure(StorageError::InvalidReplay, {}, replay.error())
            );
        }
        return std::move(*replay);
    }

    ListResult Storage::list() const {
        auto directoryResult = ensureDirectory(m_directory);
        if (!directoryResult) return std::unexpected(directoryResult.error());

        std::vector<std::string> files;
        std::error_code error;
        std::filesystem::directory_iterator iterator(
            m_directory,
            std::filesystem::directory_options::skip_permission_denied,
            error
        );
        std::filesystem::directory_iterator end;
        if (error) {
            return std::unexpected(failure(StorageError::DirectoryUnavailable, error.message()));
        }

        while (iterator != end) {
            auto const& entry = *iterator;
            auto status = entry.symlink_status(error);
            if (error) {
                return std::unexpected(failure(StorageError::DirectoryUnavailable, error.message()));
            }
            if (!std::filesystem::is_symlink(status) && std::filesystem::is_regular_file(status)) {
                auto name = pathFileName(entry.path());
                if (endsWithTtrl(name)) {
                    if (files.size() == MaximumReplayFiles) {
                        return std::unexpected(failure(StorageError::TooManyFiles));
                    }
                    files.push_back(std::move(name));
                }
            }

            iterator.increment(error);
            if (error) {
                return std::unexpected(failure(StorageError::DirectoryUnavailable, error.message()));
            }
        }

        std::sort(files.begin(), files.end());
        return files;
    }

    std::string_view errorMessage(StorageError error) {
        switch (error) {
            case StorageError::DirectoryUnavailable:
                return "The replay directory is unavailable";
            case StorageError::TooManyFiles: return "There are too many replay files";
            case StorageError::FileNotFound: return "The replay file was not found";
            case StorageError::InvalidFile: return "The replay path is not a regular file";
            case StorageError::FileTooLarge: return "The replay file is too large";
            case StorageError::OpenFailed: return "The replay file could not be opened";
            case StorageError::ReadFailed: return "The replay file could not be read";
            case StorageError::WriteFailed: return "The replay file could not be written";
            case StorageError::RenameFailed: return "The replay file could not be finalized";
            case StorageError::InvalidReplay: return "The replay data is invalid";
        }
        return "The replay operation failed";
    }

    std::string describe(StorageFailure const& failure) {
        std::string message(errorMessage(failure.error));
        if (failure.codec) {
            message += ": ";
            message += errorMessage(failure.codec->error);
        }
        if (!failure.detail.empty()) {
            message += ": ";
            message += failure.detail;
        }
        return message;
    }
}

#pragma once

#include "TtrlCodec.hpp"

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace toasty::replay::ttrl {
    enum class StorageError {
        DirectoryUnavailable,
        TooManyFiles,
        FileNotFound,
        InvalidFile,
        FileTooLarge,
        OpenFailed,
        ReadFailed,
        WriteFailed,
        RenameFailed,
        InvalidReplay
    };

    struct StorageFailure {
        StorageError error;
        std::string detail;
        std::optional<CodecFailure> codec;
    };

    using SaveResult = std::expected<std::string, StorageFailure>;
    using LoadResult = std::expected<Replay, StorageFailure>;
    using ListResult = std::expected<std::vector<std::string>, StorageFailure>;

    class Storage {
    public:
        explicit Storage(std::filesystem::path directory);

        std::filesystem::path const& directory() const;
        SaveResult save(std::string_view name, Replay const& replay) const;
        LoadResult load(std::string_view fileName) const;
        ListResult list() const;

    private:
        std::filesystem::path m_directory;
    };

    std::filesystem::path defaultReplayDirectory();
    std::string_view errorMessage(StorageError error);
    std::string describe(StorageFailure const& failure);
}

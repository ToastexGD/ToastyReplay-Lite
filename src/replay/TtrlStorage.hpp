#pragma once

#include "TtrlCodec.hpp"

#include <asp/fs.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/utils/ZStringView.hpp>
#include <Geode/Result.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace toasty::replay::ttrl {
    constexpr size_t MaximumReplayName = 80;

    enum class StorageError {
        DirectoryUnavailable,
        TooManyFiles,
        FileNotFound,
        NameTaken,
        InvalidFile,
        FileTooLarge,
        OpenFailed,
        ReadFailed,
        WriteFailed,
        RenameFailed,
        DeleteFailed,
        InvalidReplay
    };

    struct StorageFailure {
        StorageError error;
        std::string detail;
        std::optional<codec::CodecFailure> codec;
    };

    using SaveResult = geode::Result<std::string, StorageFailure>;
    using LoadResult = geode::Result<Replay, StorageFailure>;
    using ListResult = geode::Result<std::vector<std::string>, StorageFailure>;
    using RemoveResult = geode::Result<void, StorageFailure>;
    using RenameResult = geode::Result<std::string, StorageFailure>;

    class Storage {
      public:
        explicit Storage(asp::fs::path directory);

        asp::fs::path const& directory() const;
        SaveResult save(geode::ZStringView name, Replay const& replay) const;
        LoadResult load(geode::ZStringView fileName) const;
        RemoveResult remove(geode::ZStringView fileName) const;
        RenameResult rename(geode::ZStringView fileName, geode::ZStringView name) const;
        ListResult list() const;

      private:
        asp::fs::path m_directory;
    };

    asp::fs::path defaultReplayDirectory();
    std::string displayName(geode::ZStringView fileName);
    geode::ZStringView errorMessage(StorageError error);
    std::string describe(StorageFailure const& failure);
} // namespace toasty::replay::ttrl

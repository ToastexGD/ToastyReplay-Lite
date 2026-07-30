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

    using SaveResult = geode::Result<std::string, StorageFailure>;
    using LoadResult = geode::Result<Replay, StorageFailure>;
    using ListResult = geode::Result<std::vector<std::string>, StorageFailure>;

    class Storage {
      public:
        explicit Storage(asp::fs::path directory);

        asp::fs::path const& directory() const;
        SaveResult save(geode::ZStringView name, Replay const& replay) const;
        LoadResult load(geode::ZStringView fileName) const;
        ListResult list() const;

      private:
        asp::fs::path m_directory;
    };

    asp::fs::path defaultReplayDirectory();
    std::string_view errorMessage(StorageError error);
    std::string describe(StorageFailure const& failure);
} // namespace toasty::replay::ttrl

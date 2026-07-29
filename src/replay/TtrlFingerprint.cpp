#include "TtrlFingerprint.hpp"

namespace {
    constexpr uint64_t Offset = 14695981039346656037ull;
    constexpr uint64_t Prime = 1099511628211ull;
}

namespace toasty::replay::ttrl {
    uint64_t fingerprintLevelData(std::string_view levelData) {
        if (levelData.empty()) return 0;

        uint64_t value = Offset;
        for (auto byte : levelData) {
            value ^= static_cast<unsigned char>(byte);
            value *= Prime;
        }
        return value;
    }

    bool matchesLevelFingerprint(uint64_t expected, std::string_view levelData) {
        return expected != 0 && expected == fingerprintLevelData(levelData);
    }
}

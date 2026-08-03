#include "TtrlFingerprint.hpp"
#include <asp/iter.hpp>

namespace toasty::replay::ttrl {
    constexpr uint64_t Offset = 14695981039346656037ull;
    constexpr uint64_t Prime = 1099511628211ull;

    uint64_t fingerprintLevelData(std::string_view levelData) {
        if (levelData.empty())
            return 0;

        uint64_t value = Offset;
        for (char byte : asp::iter::from(levelData)) {
            value ^= static_cast<unsigned char>(byte);
            value *= Prime;
        }
        return value;
    }

    bool matchesLevelFingerprint(uint64_t expected, std::string_view levelData) {
        return expected != 0 && expected == fingerprintLevelData(levelData);
    }
} // namespace toasty::replay::ttrl

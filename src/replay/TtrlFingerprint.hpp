#pragma once

#include <cstdint>
#include <string_view>

namespace toasty::replay::ttrl {
    uint64_t fingerprintLevelData(std::string_view levelData);
    bool matchesLevelFingerprint(uint64_t expected, std::string_view levelData);
}

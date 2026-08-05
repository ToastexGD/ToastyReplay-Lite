#pragma once

#include <cstdint>
#include <string>

namespace toasty::tps {
    constexpr int64_t Vanilla = 240;
    constexpr int64_t Minimum = 1;
    constexpr int64_t Maximum = 1000000;

    bool available();
    bool enabled();
    bool setEnabled(bool value);
    int64_t rate();
    int64_t effectiveRate();
    void setRate(int64_t value);
    bool beginReplayOverride(int64_t value);
    void endReplayOverride();
    std::string const& unavailableReason();
} // namespace toasty::tps

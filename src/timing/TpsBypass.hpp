#pragma once

#include <cstdint>
#include <string>

namespace toasty::tps {
    constexpr int64_t Minimum = 240;
    constexpr int64_t Maximum = 1000000;

    bool available();
    bool enabled();
    bool setEnabled(bool value);
    int64_t rate();
    void setRate(int64_t value);
    std::string const& unavailableReason();
} // namespace toasty::tps

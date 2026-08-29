#pragma once

#include <Geode/Geode.hpp>
#include <cstdint>
#include <string>

namespace toasty::tps::patch {
    bool initialize();
    bool available();
    bool staticPatch();
    bool interceptsTicks();
    bool setEnabled(bool enabled);
    void setExpected(uint32_t ticks);
    std::string const& error();
} // namespace toasty::tps::patch

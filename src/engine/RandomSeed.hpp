#pragma once

#include <cstdint>

class GJBaseGameLayer;

namespace toasty::seed {
    bool enabled();
    uint64_t value();
    void setValue(uint64_t value);
    void apply(GJBaseGameLayer* layer, uint64_t value);
} // namespace toasty::seed

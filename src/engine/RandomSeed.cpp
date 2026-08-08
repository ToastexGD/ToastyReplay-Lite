#include "RandomSeed.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/GameToolbox.hpp>
#include <Geode/binding/GJBaseGameLayer.hpp>

using namespace geode::prelude;

namespace toasty::seed {
    bool enabled() {
        return Mod::get()->getSavedValue<bool>("set-seed", false);
    }

    uint64_t value() {
        return Mod::get()->getSavedValue<uint64_t>("seed-value", 1);
    }

    void setValue(uint64_t value) {
        Mod::get()->setSavedValue<uint64_t>("seed-value", value);
    }

    void apply(GJBaseGameLayer* layer, uint64_t value) {
        if (!layer) {
            return;
        }
        layer->m_randomSeed = value;
        layer->m_replayRandSeed = value;
        GameToolbox::fast_srand(value);
    }
} // namespace toasty::seed

#include "TtrlStorage.hpp"

#include <Geode/loader/Mod.hpp>

namespace toasty::replay::ttrl {
    std::filesystem::path defaultReplayDirectory() {
        return geode::Mod::get()->getSaveDir() / "replays";
    }
}

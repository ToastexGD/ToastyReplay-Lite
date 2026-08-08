#include "Notifications.hpp"

#include <Geode/Geode.hpp>

#include <utility>

using namespace geode::prelude;

namespace toasty::notifications {
    void show(std::string message, NotificationIcon icon) {
        if (!Mod::get()->getSavedValue<bool>("show-notifications", true)) {
            return;
        }
        Notification::create(std::move(message), icon)->show();
    }
}

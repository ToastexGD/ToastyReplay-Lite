#pragma once

#include <Geode/ui/Notification.hpp>

#include <string>

namespace toasty::notifications {
    void show(std::string message, geode::NotificationIcon icon);
}

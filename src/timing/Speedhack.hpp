#pragma once

namespace toasty::speedhack {
    constexpr double Default = 1.0;
    constexpr double Minimum = 0.01;
    constexpr double Maximum = 1000.0;

    bool enabled();
    double rate();
    void setEnabled(bool value);
    void setRate(double value);
    void syncAudio();
}

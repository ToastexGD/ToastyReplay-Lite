#pragma once

namespace toasty::stepper {
    bool enabled();
    void setEnabled(bool value);
    bool freezes();
    void step();
    bool takeStep();
} // namespace toasty::stepper

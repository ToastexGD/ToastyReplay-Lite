#pragma once

namespace toasty::stepper {
    bool enabled();
    void setEnabled(bool value);
    bool overridesTps();
    bool freezes();
    void stepOnce();
    void setRepeating(bool repeating);
    bool takeStep();
} // namespace toasty::stepper

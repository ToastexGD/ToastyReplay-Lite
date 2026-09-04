#pragma once

namespace toasty::stepper {
    bool enabled();
    void setEnabled(bool value);
    bool sessionOpen();
    void openSession();
    void closeSession();
    bool paused();
    void setPaused(bool value);
    bool overridesTps();
    bool freezes();
    void syncMusic();
    void stepOnce();
    void setKeyHeld(bool held);
    void setButtonHeld(bool held);
    bool takeStep();
} // namespace toasty::stepper

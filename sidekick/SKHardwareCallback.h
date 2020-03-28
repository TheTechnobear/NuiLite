#pragma once

#include "NuiDevice.h"


class SKApp;

class SKHardwareCallback : public NuiLite::NuiCallback {
public:
    explicit SKHardwareCallback(SKApp &app);
    void init();
    void onButton(unsigned id, unsigned value) override;
    void onEncoder(unsigned id, int value) override;

private:
    SKApp &app_;
};


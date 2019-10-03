#pragma once

#include "NuiDevice.h"


class SKApp;

class SKCallback : public NuiLite::NuiCallback {
public:
    explicit SKCallback(SKApp &app);
    void init();
    void onButton(unsigned id, unsigned value) override;
    void onEncoder(unsigned id, int value) override;

private:
    SKApp &app_;
};


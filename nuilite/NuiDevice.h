#pragma once

#include <string>
#include <memory>

namespace NuiLite {


class NuiCallback {
public:
    NuiCallback() { ; }

    virtual ~NuiCallback() = default;
    virtual void onButton(unsigned id, unsigned value) = 0;
    virtual void onEncoder(unsigned id, int value) = 0;
};


class NuiDeviceImpl_;

class NuiDevice {
public:
    NuiDevice(const char *resourcePath = nullptr);
    ~NuiDevice();

    void start();
    void stop();
    unsigned process(bool paint = true);
    void addCallback(std::shared_ptr<NuiCallback>);

    bool buttonState(unsigned but);
    unsigned numEncoders();

    void displayClear();
    void displayPaint();
    // draw funcs
    void clearRect(unsigned clr, unsigned x, unsigned y, unsigned w, unsigned h);
    void drawText(unsigned clr, unsigned x, unsigned y, const std::string &str);
    void drawPNG(unsigned x, unsigned y, const char *filename);

    // simple text displays
    void displayText(unsigned clr, unsigned line, unsigned col, const std::string &str);
    void clearText(unsigned clr, unsigned line);
    void invertText(unsigned line);
private:
    NuiDeviceImpl_ *impl_;
};


}

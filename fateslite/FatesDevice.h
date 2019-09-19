#pragma once
#include <string>
#include <memory>

namespace FatesLite {


class FatesCallback {
public:
    FatesCallback() {;}
    virtual ~FatesCallback() = default;
    virtual void onButton(unsigned id, unsigned value) = 0;
    virtual void onEncoder(unsigned id, int value) =0;
};


class FatesDeviceImpl_;

class FatesDevice {
public:
    FatesDevice(const char* resourcePath=nullptr);
    ~FatesDevice();

    void start();
    void stop();
    unsigned process();
    void addCallback(std::shared_ptr<FatesCallback>);

    void displayClear();
    // simple text displays
    void displayText(unsigned line,const std::string& str);
    void displayText(unsigned line,unsigned col,const std::string& str);
    void invertText(unsigned line);
    void clearText(unsigned line);
    void drawPNG(unsigned x, unsigned y, const char* filename);
private:
    FatesDeviceImpl_* impl_;
};


}

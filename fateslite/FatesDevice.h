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
    void displayLine(unsigned line,const std::string& str);
    void invertLine(unsigned line);
    void clearLine(unsigned line);

private:
    FatesDeviceImpl_* impl_;
};


}
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
    void displayLine(int x , int y , const std::string& str);

private:
    FatesDeviceImpl_* impl_;
};


}
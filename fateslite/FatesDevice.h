#pragma once
#include <string>
#include <memory>

namespace FatesLite {


class FatesCallback {
    virtual ~FatesCallback() = default;
    virtual void onEncoder(unsigned id, unsigned value) { ; }
    virtual void onButton(unsigned id, unsigned value) { ; }
};


class FatesDeviceImpl_;

class FatesDevice {
public:
    FatesDevice();
    ~FatesDevice();

    void start();
    void stop();
    unsigned process();
    void addCallback(std::shared_ptr<FatesCallback>);

    void displayLine(int x , int y , const std::string& str);

private:
    FatesDeviceImpl_* impl_;
};


}
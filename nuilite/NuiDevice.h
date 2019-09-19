#pragma once
#include <string>
#include <memory>

namespace NuiLite {


class NuiCallback {
public:
    NuiCallback() {;}
    virtual ~NuiCallback() = default;
    virtual void onButton(unsigned id, unsigned value) = 0;
    virtual void onEncoder(unsigned id, int value) =0;
};


class NuiDeviceImpl_;

class NuiDevice {
public:
    NuiDevice(const char* resourcePath=nullptr);
    ~NuiDevice();

    void start();
    void stop();
    unsigned process();
    void addCallback(std::shared_ptr<NuiCallback>);

    void displayClear();
    // draw funcs
    void clearRect(unsigned x, unsigned y, unsigned w, unsigned h,  unsigned clr);
    void drawText(unsigned x, unsigned y, const std::string& str, unsigned clr);
    void drawPNG(unsigned x, unsigned y, const char* filename);
    
    // simple text displays
    void displayText(unsigned line,const std::string& str);
    void displayText(unsigned line,unsigned col,const std::string& str);
    void displayText(unsigned line,unsigned col,const std::string& str,unsigned clr);
    void clearText(unsigned line);
    void clearText(unsigned line,unsigned clr);
    void invertText(unsigned line);
private:
    NuiDeviceImpl_* impl_;
};


}

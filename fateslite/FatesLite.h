#pragma once

#include <string>
#include "FatesLib.h"

namespace FatesLite {

class FatesDevice {
public:
    void init();
    void deInit();
    void start();
    void stop();
    void process();


    void displayLine(int x , int y , const std::string& str);
private:
    FatesLib lib_;
};

}


#include "FatesLite.h"


namespace FatesLite {

void FatesDevice::init() {
    lib_.init();
}

void FatesDevice::deInit() {
    lib_.deinit();
}

void FatesDevice::start() {
    ;
}

void FatesDevice::stop() {
    ;
}

void FatesDevice::process() {
    lib_.poll();
}


void FatesDevice::displayLine(int x , int y , const std::string& str) {
    lib_.displayLine(x,y,str);
}

}

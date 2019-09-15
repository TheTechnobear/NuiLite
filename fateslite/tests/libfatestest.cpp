#include "FatesDevice.h"
#include <iostream>
#include <unistd.h>
#include <iomanip>
#include <signal.h>

static volatile bool keepRunning = 1;


FatesLite::FatesDevice device;

void intHandler(int dummy) {
    std::cerr << "LibFatesTest intHandler called" << std::endl;
    if(!keepRunning) {
        sleep(1);
        exit(-1);
    }
    keepRunning = false;
    device.stop();
}

class TestCallback : public FatesLite::FatesCallback {
public:
    virtual void onButton(unsigned id, unsigned value)  {
        std::cerr << "button " << id << ":" << value << std::endl;
    }

    virtual void onEncoder(unsigned id, int value)  {
        std::cerr << "encoder " << id << ":" << value << std::endl;
    }
};

int main(int argc, const char * argv[]) {
    std::cout << "starting test" << std::endl;
    device.addCallback(std::make_shared<TestCallback>());

    device.start();

    signal(SIGINT, intHandler);


    char buf[100];
    std::cout << "started test" << std::endl;
    unsigned int i=0;
    while(keepRunning) {
        sprintf(buf,"hello, world %d", i++);
        device.displayLine(32,32,buf);
        device.process();
        sleep(1);
    }
    std::cout << "stopping test" << std::endl;
    device.stop();
    return 0;
}

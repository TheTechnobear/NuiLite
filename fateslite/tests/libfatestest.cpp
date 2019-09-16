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
    void onButton(unsigned id, unsigned value)  override {
	char buf[100];
        sprintf(buf,"Button %d : %d", id, value);
        device.displayLine(32,10,buf);
        fprintf(stderr,"button %d : %d\n", id, value);
    }

    void onEncoder(unsigned id, int value) override  {
	char buf[100];
        sprintf(buf,"Encoder %d : %d", id, value);
        device.displayLine(32,20,buf);
        fprintf(stderr,"encoder %d : %d\n", id, value);
    }
};


// note: we are only painting every second to avoid tight loop here 

int main(int argc, const char * argv[]) {
    std::cout << "starting test" << std::endl;
    device.addCallback(std::make_shared<TestCallback>());

    device.start();

    signal(SIGINT, intHandler);

    std::cout << "started test" << std::endl;
    while(keepRunning) {
        device.displayClear();
        device.process();
        sleep(1);
    }
    std::cout << "stopping test" << std::endl;
    device.stop();
    return 0;
}

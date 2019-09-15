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

char buf[100];

int lastButton=0,lastButtonState=0;
int lastEncoder=0,lastEncoderState=0;

class TestCallback : public FatesLite::FatesCallback {
public:
    virtual void onButton(unsigned id, unsigned value)  {
        lastButton = id;
        lastButtonState = value;
        fprintf(stderr,"button %d : %d\n", id, value);
    }

    virtual void onEncoder(unsigned id, int value)  {
        lastEncoder = id;
        lastEncoderState = value;
        fprintf(stderr,"encoder %d : %d\n", id, value);
    }
};

int main(int argc, const char * argv[]) {
    std::cout << "starting test" << std::endl;
    device.addCallback(std::make_shared<TestCallback>());

    device.start();

    signal(SIGINT, intHandler);

    sprintf(buf,"hello, world");

    std::cout << "started test" << std::endl;
    while(keepRunning) {
        device.displayClear();

        sprintf(buf,"Button %d : %d", lastButton, lastButtonState);
        device.displayLine(32,10,buf);

        sprintf(buf,"Encoder %d : %d", lastEncoder, lastEncoderState);
        device.displayLine(32,20,buf);
        device.process();
        sleep(1);
        // since we dont get a msg for stop turning assume it has!
        lastEncoderState=0;
    }
    std::cout << "stopping test" << std::endl;
    device.stop();
    return 0;
}

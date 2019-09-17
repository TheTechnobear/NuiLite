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
	device.clearText(0);
        device.displayText(0,buf);
        fprintf(stderr,"button %d : %d\n", id, value);
        if(value) {
            switch(id) {
                case 0 : {
			device.clearText(3);
			device.displayText(3,0,"hello");
			device.displayText(3,25,"world");
			device.displayText(4,10,"world");
			break;
		}
                case 1 : device.clearText(3); break;
                case 2 : device.invertText(3); break;
                default: break;

            }
        }
    }

    void onEncoder(unsigned id, int value) override  {
	char buf[100];
        sprintf(buf,"Encoder %d : %d ", id, value);
	device.clearText(1);
        device.displayText(1,buf);
        fprintf(stderr,"encoder %d : %d\n", id, value);
    }
};


// note: we are only painting every second to avoid tight loop here 

int main(int argc, const char * argv[]) {
    std::cout << "starting test" << std::endl;
    device.addCallback(std::make_shared<TestCallback>());

    device.start();

    signal(SIGINT, intHandler);
    device.displayClear();
    device.displayText(3,"Hello world");

    std::cout << "started test" << std::endl;
    while(keepRunning) {
        device.process();
        sleep(1);
    }
    std::cout << "stopping test" << std::endl;
    device.stop();
    return 0;
}

#include "NuiDevice.h"
#include <iostream>
#include <unistd.h>
#include <iomanip>
#include <signal.h>

static volatile bool keepRunning = 1;


NuiLite::NuiDevice device;

void intHandler(int dummy) {
    std::cerr << "LibNuiTest intHandler called" << std::endl;
    if (!keepRunning) {
        sleep(1);
        exit(-1);
    }
    keepRunning = false;
}


class TestCallback : public NuiLite::NuiCallback {
public:
    void onButton(unsigned id, unsigned value) override {
        char buf[100];
        sprintf(buf, "Button %d : %d", id, value);
        device.clearText(1);
        device.displayText(15, 0, 1, buf);
        fprintf(stderr, "button %d : %d\n", id, value);
        if (value) {
            switch (id) {
                case 0 : {
                    device.clearText(0, 2);
                    device.displayText(0, 2, 0, "hello");
                    device.displayText(0, 2, 25, "world");
                    break;
                }
                case 1 :
                    device.clearText(0, 2);
                    break;
                case 2 :
                    device.invertText(2);
                    break;
                default:
                    break;

            }
        }
    }

    void onEncoder(unsigned id, int value) override {
        char buf[100];
        sprintf(buf, "Encoder %d : %d ", id, value);
        device.clearText(0);
        device.displayText(15, 0, 0, buf);
        fprintf(stderr, "encoder %d : %d\n", id, value);
    }
};


// note: we are only painting every second to avoid tight loop here 
void funcParam(unsigned row, unsigned col, const std::string &name, const std::string &value, bool selected = false) {
    unsigned x = col * 64;
    unsigned y1 = (row + 1) * 20;
    unsigned y2 = y1 + 10;
    unsigned clr = selected ? 15 : 0;
    device.clearRect(5, x, y1, 62 + (col * 2), -10);
    device.drawText(clr, x + 1, y1 - 1, name);
    device.clearRect(0, x, y2, 62 + (col * 2), -10);
    device.drawText(15, x + 1, y2 - 1, value);
}

int main(int argc, const char *argv[]) {
    std::cout << "starting test" << std::endl;
    device.addCallback(std::make_shared<TestCallback>());

    device.start();

    signal(SIGINT, intHandler);
    device.displayClear();


    device.drawPNG(0, 0, "./orac.png");
    device.clearText(1, 0);
    device.displayText(15, 0, 0, "a1 : BasicPoly > main");

    /*
    device.clearRect(0,0, 128,10,1);
    device.drawText(0,8,"a1 : BasicPoly > main",15);
    funcParam(0,0,"Transpose","12     st");
    funcParam(1,0,"Cutoff","15000 hz");
    funcParam(0,1,"Shape","33",true);
    funcParam(1,1,"Envelope","58     %");

    */

    std::cout << "started test" << std::endl;

    std::cout << "buttons : "
              << device.buttonState(0)
              << device.buttonState(1)
              << device.buttonState(2)
              << std::endl;

    while (keepRunning) {
        device.process();
        sleep(1);
    }
    std::cout << "stopping test" << std::endl;
    device.stop();
    return 0;
}

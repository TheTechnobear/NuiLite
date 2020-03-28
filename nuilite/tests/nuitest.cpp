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

void test0();
void test1();
void test2();
void test3();
void test4();
void test5();
void test6();
void test7();
void test8();
void test9();

static constexpr unsigned NTESTS = 10;


void (*tests[NTESTS])(void) = {
    test0,
    test1,
    test2,
    test3,
    test4,
    test5,
    test6,
    test7,
    test8,
    test9
};


static int testIdx = 0;

void nextTest() {
    testIdx++;
    testIdx = testIdx >= NTESTS ? testIdx = 0 : testIdx;
    fprintf(stderr, "test %d\n", testIdx);
    (*tests[testIdx])();
}

void prevTest() {
    device.displayClear();
    testIdx--;
    testIdx = testIdx < 0 ? NTESTS - 1 : testIdx;
    fprintf(stderr, "test %d\n", testIdx);
    (*tests[testIdx])();
}


class TestCallback : public NuiLite::NuiCallback {
public:
    void onButton(unsigned id, unsigned value) override {
        fprintf(stderr, "button %d : %d\n", id, value);
        if (value) {
            switch (id) {
                case 0 : {
                    device.displayClear();
                    break;
                }
                case 1 :
                    prevTest();
                    break;
                case 2 :
                    nextTest();
                    break;
                default:
                    break;

            }
        }
    }

    void onEncoder(unsigned id, int value) override {
        char buf[100];
        sprintf(buf, "Encoder %d : %d ", id, value);
        device.clearText(0, 0);
        device.displayText(15, 0, 1, buf);
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

    std::cout << "started test" << std::endl;
    std::cout << "buttons : "
              << device.buttonState(0)
              << device.buttonState(1)
              << device.buttonState(2)
              << std::endl;

    device.displayClear();
    (*tests[0])();

    while (keepRunning) {
        device.process();
        sleep(1);
    }
    std::cout << "stopping test" << std::endl;
    device.stop();
    return 0;
}



/// tests

void test0() {
    device.displayClear();

    device.clearLine(0, 0);
    device.textLine(15, 0, 0, "test 0");
    device.gPng(0, 0, "./orac.png");

}

void test1() {
    device.clearLine(0, 0);
    device.textLine(15, 0, 0, "test 1");
    device.clearText(1, 0);
    device.displayText(15, 0, 0, "a1 : BasicPoly > main");
}

void test2() {
    device.displayClear();

    device.clearLine(0, 0);
    device.textLine(15, 0, 0, "test 2 - text line");
    device.textLine(15, 1, 0, "line 1");
    device.textLine(15, 2, 0, "line 2");
    device.textLine(15, 3, 0, "line 3");
    device.textLine(15, 4, 0, "line 4");
}

void test3() {
    device.clearLine(0, 0);
    device.textLine(15, 0, 0, "test 3 - inv/clr line");
    device.invertLine(1);
    device.clearLine(0, 2);
}

void test4() {
    device.clearLine(0, 0);
    device.textLine(15, 0, 0, "test 4 0 - gTest");

    device.gText(15, 32, 32, "@ 32 32");
}

void test5() {
    device.clearLine(0, 0);
    device.textLine(15, 0, 0, "test 5 - gRectangel");
    device.gRectangle(15, 5, 5, 40, 40);
}

void test6() {
    device.clearLine(0, 0);
    device.textLine(15, 0, 0, "test 6 - gCircle");
    device.gCircle(15, 64, 32, 10);
}

void test7() {
    device.clearLine(0, 0);
    device.textLine(15, 0, 0, "test 7 - gFilledCircle");
    device.gFilledCircle(15, 64, 32, 10);
}

void test8() {
    device.clearLine(0, 0);
    device.textLine(15, 0, 0, "test 8 - gLine");
    device.gLine(15, 0, 0, 64, 32);
}

#include <math.h>
#include <vector>

void test9() {
    device.clearLine(0, 0);
    device.textLine(15, 0, 0, "test 9");
    std::vector<unsigned> wave;
    for (int x = 0; x < 127; x++) {
        double i = float(x) * ((2.0 * M_PI) / 127.0);
        unsigned y = unsigned((sin(i) * 15.0) + 25.0);
        wave.push_back(y);
    }
    device.gWaveform(15, wave);
    device.gInvert();
}

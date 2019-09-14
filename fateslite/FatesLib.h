#pragma once
#include <pthread.h>
#include <cairo.h>
#include <string>

namespace FatesLite {

class FatesLib {
public:
    void init();
    void deinit();
    void poll();

    void displayLine(int x , int y , const std::string& str);

public:
    // await refactor etc
    static constexpr unsigned num_enc = 4;
    static int key_fd;
    static int enc_fd[num_enc];


private:
    pthread_t key_p;
    pthread_t enc_p[num_enc];

    void initGPIO();
    void deinitGPIO();
    void initDisplay();
    void deinitDisplay();


    void displayPaint();


    cairo_surface_t *surfacefb_;
    cairo_t* crfb_;
    cairo_surface_t* surface_;
    cairo_t*	cr_;
};


}
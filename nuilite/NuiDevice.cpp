#include "NuiDevice.h"

#include <memory>
#include <vector>
#include <thread>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <cairo.h>
#include <cairo-ft.h>

// gpio
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include <linux/input.h>
#include <time.h>
#include <sys/poll.h>
#include <sys/stat.h>

#include <readerwriterqueue.h>


#ifndef SCREEN_FB_FMT
#define SCREEN_FB_FMT CAIRO_FORMAT_RGB16_565
#endif

#ifndef SCREEN_FMT
#define SCREEN_FMT CAIRO_FORMAT_ARGB32
#endif

#ifndef SCREEN_X
#define SCREEN_X 128
#endif

#ifndef SCREEN_Y
#define SCREEN_Y 64
#endif

#ifndef SCREEN_SCALE
#define SCREEN_SCALE 1.0
#endif

#define NUM_FONTS 67
static cairo_font_face_t *ct[NUM_FONTS];

static float colours[16] =
    {0.0f, 0.066666666666667f, 0.13333333333333f, 0.2f, 0.26666666666667f,
     0.33333333333333f,
     0.4f, 0.46666666666667f, 0.53333333333333f, 0.6f, 0.66666666666667f,
     0.73333333333333f, 0.8f, 0.86666666666667f, 0.93333333333333f, 1.0f};

namespace NuiLite {

struct NuiEventMsg {
    enum {
        N_BUTTON,
        N_ENCODER
    } type_;
    unsigned id_;
    int value_;
};

// implementation class
class NuiDeviceImpl_ {
public:
    NuiDeviceImpl_(const char *resPath);
    ~NuiDeviceImpl_();

    void start();
    void stop();
    unsigned process(bool);
    void addCallback(std::shared_ptr<NuiCallback>);

    bool buttonState(unsigned but);
    unsigned numEncoders();


    void displayClear();
    void displayPaint();

    // text functions 
    void displayText(unsigned clr, unsigned line, unsigned col, const std::string &str);
    void clearText(unsigned clr, unsigned line);
    void invertText(unsigned line);

    // draw functions
    void clearRect(unsigned clr, unsigned x, unsigned y, unsigned w, unsigned h);
    void drawText(unsigned clr, unsigned x, unsigned y, const std::string &str);
    void drawPNG(unsigned x, unsigned y, const char *filename);

    // public but not part of interface!
    void processGPIO();

private:
    void initGPIO();
    void deinitGPIO();
    void initDisplay();
    void deinitDisplay();


    std::string resPath_;
    std::thread gpioThread_;
    bool keepRunning_;

    static constexpr unsigned MAX_EVENTS = 64;
    static constexpr unsigned MAX_ENCODERS = 4;
    int keyFd_;
    int encFd_[MAX_ENCODERS];
    unsigned numEncoders_=4;
    std::string fbDev_;

    cairo_surface_t *surfacefb_;
    cairo_t *crfb_;
    cairo_surface_t *surface_;
    cairo_t *cr_;
    std::vector<std::shared_ptr<NuiCallback>> callbacks_;
    moodycamel::ReaderWriterQueue<NuiEventMsg> eventQueue_;
};

//NuiDevice proxy
NuiDevice::NuiDevice(const char *resPath) {
    impl_ = new NuiDeviceImpl_(resPath);
}

NuiDevice::~NuiDevice() {
    if (impl_) {
        delete impl_;
        impl_ = nullptr;
    }
}

void NuiDevice::start() {
    impl_->start();
}

void NuiDevice::stop() {
    impl_->stop();
}

unsigned NuiDevice::process(bool paint) {
    return impl_->process(paint);
}

void NuiDevice::addCallback(std::shared_ptr<NuiCallback> cb) {
    impl_->addCallback(cb);

}

bool NuiDevice::buttonState(unsigned but) {
    return impl_->buttonState(but);
}

unsigned NuiDevice::numEncoders() {
    return impl_->numEncoders();
}


void NuiDevice::displayClear() {
    impl_->displayClear();
}

void NuiDevice::displayPaint() {
    impl_->displayPaint();
}

void NuiDevice::displayText(unsigned clr, unsigned line, unsigned col, const std::string &str) {
    impl_->displayText(clr, line, col, str);
}

void NuiDevice::invertText(unsigned line) {
    impl_->invertText(line);
}

void NuiDevice::clearText(unsigned clr, unsigned line) {
    impl_->clearText(clr, line);
}

void NuiDevice::drawPNG(unsigned x, unsigned y, const char *filename) {
    impl_->drawPNG(x, y, filename);
}

void NuiDevice::clearRect(unsigned clr, unsigned x, unsigned y, unsigned w, unsigned h) {
    impl_->clearRect(clr, x, y, w, h);
}

void NuiDevice::drawText(unsigned clr, unsigned x, unsigned y, const std::string &str) {
    impl_->drawText(clr, x, y, str);
}


// fwd decl for helper functions
extern void cairo_linuxfb_surface_destroy(void *device);
extern cairo_surface_t *cairo_linuxfb_surface_create(const char* fbdev);
extern int opengpio(const char *pathname, int flags);
void setup_local_fonts(const char *);


//// start of IMPLEMENTATION


NuiDeviceImpl_::NuiDeviceImpl_(const char *resPath) : eventQueue_(MAX_EVENTS), resPath_(resPath == nullptr ? "" : resPath) {
    if (resPath_.size() == 0) resPath_ = "/home/we/norns/resources";
}

NuiDeviceImpl_::~NuiDeviceImpl_() {
    ;
}

void NuiDeviceImpl_::addCallback(std::shared_ptr<NuiCallback> cb) {
    callbacks_.push_back(cb);
}

void NuiDeviceImpl_::start() {
    keepRunning_ = true;
    initGPIO();
    initDisplay();
}

void NuiDeviceImpl_::stop() {
    keepRunning_ = false;

    deinitDisplay();
    deinitGPIO();
}

unsigned NuiDeviceImpl_::process(bool paint) {
    NuiEventMsg msg;
    while (eventQueue_.try_dequeue(msg)) {
        for (auto cb: callbacks_) {
            switch (msg.type_) {
                case NuiEventMsg::N_BUTTON : {
                    cb->onButton(msg.id_, msg.value_);
                    break;
                }
                case NuiEventMsg::N_ENCODER : {
                    cb->onEncoder(msg.id_, msg.value_);
                    break;
                }
                default:; // ignore
            }
        }
    }
    if (paint) displayPaint();
    return 0;
}


bool NuiDeviceImpl_::buttonState(unsigned but) {
    char key = 0;
    ioctl(keyFd_, EVIOCGKEY(1), &key);
    return ((unsigned) key) & (1 << but + 1);
}


unsigned NuiDeviceImpl_::numEncoders() {
    return numEncoders_;
}


//// display functions

void NuiDeviceImpl_::displayPaint() {
    cairo_paint(crfb_);

}

void NuiDeviceImpl_::clearRect(unsigned clr, unsigned x, unsigned y, unsigned w, unsigned h) {
    cairo_set_source_rgb(cr_, colours[clr], colours[clr], colours[clr]);
    cairo_rectangle(cr_, x, y, w, h);
    cairo_fill(cr_);
}


void NuiDeviceImpl_::drawText(unsigned clr, unsigned x, unsigned y, const std::string &str) {
    cairo_set_source_rgb(cr_, colours[clr], colours[clr], colours[clr]);
    cairo_move_to(cr_, x, y);
    cairo_show_text(cr_, str.c_str());
    cairo_fill(cr_);
}


void NuiDeviceImpl_::displayClear() {
    cairo_set_operator(cr_, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr_);
    cairo_set_operator(cr_, CAIRO_OPERATOR_OVER);
}

void NuiDeviceImpl_::displayText(unsigned clr, unsigned line, unsigned col, const std::string &str) {
    unsigned x = col * 4;
    unsigned y = line * 10 + 10;

    cairo_set_source_rgb(cr_, colours[clr], colours[clr], colours[clr]);
    cairo_move_to(cr_, x, y);
    cairo_show_text(cr_, str.c_str());
    cairo_fill(cr_);
}

void NuiDeviceImpl_::invertText(unsigned line) {
    unsigned x = 0;
    unsigned y = (line * 10 + 10) + 1; // letters with drop
    cairo_set_source_rgb(cr_, 1., 1., 1.);
    cairo_set_operator(cr_, CAIRO_OPERATOR_DIFFERENCE);
    cairo_rectangle(cr_, x, y + 1, SCREEN_X, -10);
    cairo_fill(cr_);
    cairo_set_operator(cr_, CAIRO_OPERATOR_OVER);
}

void NuiDeviceImpl_::clearText(unsigned clr, unsigned line) {
    unsigned x = 0;
    unsigned y = (line * 10 + 10) + 1; // letters with drop
    cairo_set_source_rgb(cr_, colours[clr], colours[clr], colours[clr]);
//    cairo_text_extents_t extents;
//    cairo_text_extents (cr_, "0", &extents);
//    cairo_rectangle(cr_,x,y,(extents.width+extents.x_bearing)*str.size()+1 ,extents.y_bearing);
    cairo_rectangle(cr_, x, y + 1, SCREEN_X, -10);
    cairo_fill(cr_);
}


void NuiDeviceImpl_::drawPNG(unsigned x, unsigned y, const char *filename) {
    int img_w, img_h;

    auto image = cairo_image_surface_create_from_png(filename);
    if (image == NULL) {
        fprintf(stderr, "failed to load: %s\n", filename);
        return;
    }
    fprintf(stderr, "loaded: %s\n", filename);

    img_w = cairo_image_surface_get_width(image);
    img_h = cairo_image_surface_get_height(image);

    cairo_set_source_surface(cr_, image, x, y);
    //cairo_paint (cr);
    cairo_rectangle(cr_, x, y, img_w, img_h);
    cairo_fill(cr_);
    cairo_set_source_surface(cr_, surface_, SCREEN_X, SCREEN_Y);
    cairo_surface_destroy(image);
}



//////  ENCODER AND KEY HANDLING //////////////////////////////////////



void *process_gpio_func(void *x) {
    auto pThis = static_cast<NuiDeviceImpl_ *>(x);
    pThis->processGPIO();
    return nullptr;
}


void NuiDeviceImpl_::processGPIO() {

    int encdir[NuiDeviceImpl_::MAX_ENCODERS] = {1, 1, 1, 1};
//    clock_t encnow[MAX_ENCODERS];
    clock_t encprev[MAX_ENCODERS];
    encprev[0] = encprev[1] = encprev[2] = encprev[3] = clock();
    struct input_event event;

    while (keepRunning_) {

        struct pollfd fds[1 + MAX_ENCODERS];

        //FIXME only works if order of fd doesnt change
        // i.e key/enc 1 to 3 must open
        unsigned fdcount = 0;
        if (keyFd_ > 0) {
            fds[fdcount].fd = keyFd_;
            fds[fdcount].events = POLLIN;
            fdcount++;
        }
        for (auto i = 0; i < numEncoders_; i++) {
            if (encFd_[i] > 0) {
                fds[fdcount].fd = encFd_[i];
                fds[fdcount].events = POLLIN;
                fdcount++;
            } else {
                fprintf(stderr, "enc %d not opened", i);
            }
        }

        auto result = poll(fds, fdcount, 5000);

        if (result > 0) {
            if (fds[0].revents & POLLIN) {
                auto rd = read(keyFd_, &event, sizeof(struct input_event));
                if (rd < (int) sizeof(struct input_event)) {
                    fprintf(stderr, "ERROR (key) read error\n");
                } else {
                    if (event.type) { // make sure it's not EV_SYN == 0
//                        fprintf(stderr, "button %d = %d\n", event.code, event.value);
                        NuiEventMsg msg;
                        msg.type_ = NuiEventMsg::N_BUTTON;
                        msg.id_ = event.code - 1; // make zerp based
                        msg.value_ = event.value;
                        eventQueue_.enqueue(msg);
                    }
                }
            }

            for (auto n = 0; n < numEncoders_; n++) {
                if (fds[n + 1].revents & POLLIN) {
                    auto rd = read(encFd_[n], &event, sizeof(struct input_event));
                    if (rd < (int) sizeof(struct input_event)) {
                        fprintf(stderr, "ERROR (enc) read error\n");
                    }

                    auto now = clock();
                    if (event.type) { // make sure it's not EV_SYN == 0
                        auto diff = now - encprev[n];
//                        fprintf(stderr, "encoder %d\t%d\t%lu\n", n, event.value, diff);
                        encprev[n] = now;
                        if (diff > 100) { // filter out glitches
                            if (encdir[n] != event.value && diff > 500) { // only reverse direction if there is reasonable settling time
                                encdir[n] = event.value;
                            }
                            NuiEventMsg msg;
                            msg.type_ = NuiEventMsg::N_ENCODER;
                            msg.id_ = n;
                            msg.value_ = event.value;
                            eventQueue_.enqueue(msg);
                        }
                    }
                }
            }
        }

    }
}


void NuiDeviceImpl_::initGPIO() {

    const char *enc_filenames[MAX_ENCODERS] = {"/dev/input/by-path/platform-soc:knob1-event",
                                               "/dev/input/by-path/platform-soc:knob2-event",
                                               "/dev/input/by-path/platform-soc:knob3-event",
                                               "/dev/input/by-path/platform-soc:knob4-event"};
    keyFd_ = opengpio("/dev/input/by-path/platform-keys-event", O_RDONLY);

    struct stat fs;
    numEncoders_ = 3;
    if ((stat("/dev/input/by-path/platform-soc:knob4-event", &fs) == 0)) {
        numEncoders_ = 4;
    }


    for (auto i = 0; i < numEncoders_; i++) {
        encFd_[i] = opengpio(enc_filenames[i], O_RDONLY);
    }

    gpioThread_ = std::thread(process_gpio_func, this);
}

void NuiDeviceImpl_::deinitGPIO() {
    keepRunning_ = false;
    if (gpioThread_.joinable()) gpioThread_.join();

}

////////////////////////    DISPLAY HANDLING ////////////////////////////////////////////



void NuiDeviceImpl_::initDisplay() {
    fbDev_ = "/dev/fb0";
    struct stat fs;
    if ((stat("/dev/fb1", &fs) == 0)) {
        fbDev_ = "/dev/fb1";
    }

    surfacefb_ = cairo_linuxfb_surface_create(fbDev_.c_str());
    crfb_ = cairo_create(surfacefb_);

    surface_ = cairo_image_surface_create(SCREEN_FMT, SCREEN_X, SCREEN_Y);
    cr_ = cairo_create(surface_);

    cairo_set_operator(crfb_, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(crfb_, surface_, 0, 0);

    setup_local_fonts(resPath_.c_str());
    // clear display
    cairo_set_operator(cr_, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr_);
    cairo_set_operator(cr_, CAIRO_OPERATOR_OVER);
    cairo_paint(crfb_);

    cairo_font_options_t *font_options = cairo_font_options_create();
    cairo_font_options_set_antialias(font_options, CAIRO_ANTIALIAS_SUBPIXEL);
    cairo_set_font_options(cr_, font_options);
    cairo_font_options_destroy(font_options);

    cairo_set_font_face(cr_, ct[0]);

    //cairo_select_font_face(cr_, "serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr_, 8.0);

    cairo_set_source_rgb(cr_, 1, 1, 1); //!!

}

void NuiDeviceImpl_::deinitDisplay() {
    cairo_destroy(cr_);
    cairo_surface_destroy(surface_);
    cairo_destroy(crfb_);
    cairo_linuxfb_surface_destroy(surfacefb_);
}


///////// helper functions

int opengpio(const char *pathname, int flags) {
    int fd;
    int open_attempts = 0, ioctl_attempts = 0;
    while (open_attempts < 200) {
        fd = open(pathname, flags);
        if (fd > 0) {
            if (ioctl(fd, EVIOCGRAB, 1) == 0) {
                ioctl(fd, EVIOCGRAB, (void *) 0);
                goto done;
            }
            ioctl_attempts++;
            close(fd);
        }
        open_attempts++;
        usleep(50000); // 50ms sleep * 200 = 10s fail after 10s
    };
    done:
    if (open_attempts > 0) {
        fprintf(stderr, "WARN opengpio GPIO '%s' required %d open attempts & %d ioctl attempts\n", pathname, open_attempts, ioctl_attempts);
    }
    return fd;
}

typedef struct _cairo_linuxfb_device {
    int fb_fd;
    unsigned char *fb_data;
    long fb_screensize;
    struct fb_var_screeninfo fb_vinfo;
    struct fb_fix_screeninfo fb_finfo;
} cairo_linuxfb_device_t;

/* Destroy a cairo surface */
void cairo_linuxfb_surface_destroy(void *device) {
    cairo_linuxfb_device_t *dev = (cairo_linuxfb_device_t *) device;

    if (dev == NULL) {
        return;
    }

    munmap(dev->fb_data, dev->fb_screensize);
    close(dev->fb_fd);
    free(dev);
}

/* Create a cairo surface using the specified framebuffer */
cairo_surface_t *cairo_linuxfb_surface_create(const char* fbdev) {
    cairo_linuxfb_device_t *device;
    cairo_surface_t *surface;

    const char *fb_name = fbdev;
    fprintf(stderr, "using framebuffer %s\n", fbdev);

    device = static_cast<cairo_linuxfb_device_t *>( malloc(sizeof(*device)));
    if (!device) {
        fprintf(stderr, "ERROR (screen) cannot allocate memory\n");
        return NULL;
    }

    // Open the file for reading and writing
    device->fb_fd = open(fb_name, O_RDWR);
    if (device->fb_fd == -1) {
        fprintf(stderr, "ERROR (screen) cannot open framebuffer device");
        goto handle_allocate_error;
    }

    // Get variable screen information
    if (ioctl(device->fb_fd, FBIOGET_VSCREENINFO, &device->fb_vinfo) == -1) {
        fprintf(stderr, "ERROR (screen) reading variable information");
        goto handle_ioctl_error;
    }

    // Figure out the size of the screen in bytes
    device->fb_screensize = device->fb_vinfo.xres * device->fb_vinfo.yres
                            * device->fb_vinfo.bits_per_pixel / 8;

    // Map the device to memory
    device->fb_data = (unsigned char *) mmap(0, device->fb_screensize,
                                             PROT_READ | PROT_WRITE, MAP_SHARED,
                                             device->fb_fd, 0);

    if (device->fb_data == (unsigned char *) -1) {
        fprintf(stderr, "ERROR (screen) failed to map framebuffer device to memory");
        goto handle_ioctl_error;
    }

    // Get fixed screen information
    if (ioctl(device->fb_fd, FBIOGET_FSCREENINFO, &device->fb_finfo) == -1) {
        fprintf(stderr, "ERROR (screen) reading fixed information");
        goto handle_ioctl_error;
    }

    /* Create the cairo surface which will be used to draw to */
    surface = cairo_image_surface_create_for_data(
        device->fb_data,
        SCREEN_FB_FMT,
        device->fb_vinfo.xres,
        device->fb_vinfo.yres,
        cairo_format_stride_for_width(
            SCREEN_FB_FMT,
            device->fb_vinfo.xres
        )
    );
    cairo_surface_set_user_data(surface, NULL, device,
                                &cairo_linuxfb_surface_destroy);

    return surface;

    handle_ioctl_error:
    fprintf(stderr, "ERROR unable to open screen");
    close(device->fb_fd);
    handle_allocate_error:
    free(device);
    return NULL;
}

static char font_path[NUM_FONTS][32];


void setup_local_fonts(const char *resPath) {
    static FT_Library value;
    static FT_Error status;
    static FT_Face face[NUM_FONTS];

    status = FT_Init_FreeType(&value);
    if (status != 0) {
        fprintf(stderr, "ERROR (screen) freetype init\n");
        return;
    }

    strcpy(font_path[0], "04B_03__.TTF");
    strcpy(font_path[1], "liquid.ttf");
    strcpy(font_path[2], "Roboto-Thin.ttf");
    strcpy(font_path[3], "Roboto-Light.ttf");
    strcpy(font_path[4], "Roboto-Regular.ttf");
    strcpy(font_path[5], "Roboto-Medium.ttf");
    strcpy(font_path[6], "Roboto-Bold.ttf");
    strcpy(font_path[7], "Roboto-Black.ttf");
    strcpy(font_path[8], "Roboto-ThinItalic.ttf");
    strcpy(font_path[9], "Roboto-LightItalic.ttf");
    strcpy(font_path[10], "Roboto-Italic.ttf");
    strcpy(font_path[11], "Roboto-MediumItalic.ttf");
    strcpy(font_path[12], "Roboto-BoldItalic.ttf");
    strcpy(font_path[13], "Roboto-BlackItalic.ttf");
    strcpy(font_path[14], "VeraBd.ttf");
    strcpy(font_path[15], "VeraBI.ttf");
    strcpy(font_path[16], "VeraIt.ttf");
    strcpy(font_path[17], "VeraMoBd.ttf");
    strcpy(font_path[18], "VeraMoBI.ttf");
    strcpy(font_path[19], "VeraMoIt.ttf");
    strcpy(font_path[20], "VeraMono.ttf");
    strcpy(font_path[21], "VeraSeBd.ttf");
    strcpy(font_path[22], "VeraSe.ttf");
    strcpy(font_path[23], "Vera.ttf");
    //------------------
    // bitmap fonts
    strcpy(font_path[24], "bmp/tom-thumb.bdf");
    // FIXME: this is totally silly...
    int i = 25;
    strcpy(font_path[i++], "bmp/creep.bdf");
    strcpy(font_path[i++], "bmp/ctrld-fixed-10b.bdf");
    strcpy(font_path[i++], "bmp/ctrld-fixed-10r.bdf");
    strcpy(font_path[i++], "bmp/ctrld-fixed-13b.bdf");
    strcpy(font_path[i++], "bmp/ctrld-fixed-13b-i.bdf");
    strcpy(font_path[i++], "bmp/ctrld-fixed-13r.bdf");
    strcpy(font_path[i++], "bmp/ctrld-fixed-13r-i.bdf");
    strcpy(font_path[i++], "bmp/ctrld-fixed-16b.bdf");
    strcpy(font_path[i++], "bmp/ctrld-fixed-16b-i.bdf");
    strcpy(font_path[i++], "bmp/ctrld-fixed-16r.bdf");
    strcpy(font_path[i++], "bmp/ctrld-fixed-16r-i.bdf");
    strcpy(font_path[i++], "bmp/scientifica-11.bdf");
    strcpy(font_path[i++], "bmp/scientificaBold-11.bdf");
    strcpy(font_path[i++], "bmp/scientificaItalic-11.bdf");
    strcpy(font_path[i++], "bmp/ter-u12b.bdf");
    strcpy(font_path[i++], "bmp/ter-u12n.bdf");
    strcpy(font_path[i++], "bmp/ter-u14b.bdf");
    strcpy(font_path[i++], "bmp/ter-u14n.bdf");
    strcpy(font_path[i++], "bmp/ter-u14v.bdf");
    strcpy(font_path[i++], "bmp/ter-u16b.bdf");
    strcpy(font_path[i++], "bmp/ter-u16n.bdf");
    strcpy(font_path[i++], "bmp/ter-u16v.bdf");
    strcpy(font_path[i++], "bmp/ter-u18b.bdf");
    strcpy(font_path[i++], "bmp/ter-u18n.bdf");
    strcpy(font_path[i++], "bmp/ter-u20b.bdf");
    strcpy(font_path[i++], "bmp/ter-u20n.bdf");
    strcpy(font_path[i++], "bmp/ter-u22b.bdf");
    strcpy(font_path[i++], "bmp/ter-u22n.bdf");
    strcpy(font_path[i++], "bmp/ter-u24b.bdf");
    strcpy(font_path[i++], "bmp/ter-u24n.bdf");
    strcpy(font_path[i++], "bmp/ter-u28b.bdf");
    strcpy(font_path[i++], "bmp/ter-u28n.bdf");
    strcpy(font_path[i++], "bmp/ter-u32b.bdf");
    strcpy(font_path[i++], "bmp/ter-u32n.bdf");
    strcpy(font_path[i++], "bmp/unscii-16-full.pcf");
    strcpy(font_path[i++], "bmp/unscii-16.pcf");
    strcpy(font_path[i++], "bmp/unscii-8-alt.pcf");
    strcpy(font_path[i++], "bmp/unscii-8-fantasy.pcf");
    strcpy(font_path[i++], "bmp/unscii-8-mcr.pcf");
    strcpy(font_path[i++], "bmp/unscii-8.pcf");
    strcpy(font_path[i++], "bmp/unscii-8-tall.pcf");
    strcpy(font_path[i++], "bmp/unscii-8-thin.pcf");

    assert(i == NUM_FONTS);

    //fprintf(stderr, "fonts: \n");
    //for (int i=0; i<NUM_FONTS; ++i) {
    // fprintf(stderr, "  %d: %s\n", i, font_path[i]);
    //}

    char filename[256];

    for (int i = 0; i < NUM_FONTS; i++) {
        // FIXME should be path relative to norns/
        snprintf(filename, 256, "%s/%s", resPath, font_path[i]);

        status = FT_New_Face(value, filename, 0, &face[i]);
        if (status != 0) {
            fprintf(stderr, "ERROR (screen) font load: %s\n", filename);
            return;
        } else {
            ct[i] = cairo_ft_font_face_create_for_ft_face(face[i], 0);
        }
    }

}


}// namespace

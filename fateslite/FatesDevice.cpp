#include "FatesDevice.h"

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
//#include <cairo-ft.h>

// gpio
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include <linux/input.h>
#include <time.h>
#include <sys/select.h>


#define FB_DEVICE "/dev/fb0"

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




namespace FatesLite {

// implementation class
class FatesDeviceImpl_ {
public:
    FatesDeviceImpl_();
    ~FatesDeviceImpl_();

    void start();
    void stop();
    unsigned process();
    void addCallback(std::shared_ptr<FatesCallback>);

    void displayLine(int x, int y, const std::string &str);

    // public but not part of interface!
    void processGPIO();

private:
//    pthread_t key_p;
//    pthread_t enc_p[num_enc];

    void initGPIO();
    void deinitGPIO();
    void initDisplay();
    void deinitDisplay();

    void displayPaint();



    std::thread gpioThread_;
    bool keepRunning_;

    static constexpr unsigned NUM_ENCODERS = 4;
    int keyFd_;
    int encFd_[NUM_ENCODERS];

    cairo_surface_t *surfacefb_;
    cairo_t *crfb_;
    cairo_surface_t *surface_;
    cairo_t *cr_;
    std::vector<std::shared_ptr<FatesCallback>> callbacks_;
};

//FatesDevice proxy
FatesDevice::FatesDevice() {
    impl_ = new FatesDeviceImpl_();
}

FatesDevice::~FatesDevice() {
    if (impl_) {
        delete impl_;
        impl_ = nullptr;
    }
}

void FatesDevice::start() {
    impl_->start();
}

void FatesDevice::stop() {
    impl_->stop();
}

unsigned FatesDevice::process() {
    return impl_->process();
}

void FatesDevice::addCallback(std::shared_ptr<FatesCallback> cb) {
    impl_->addCallback(cb);

}

void FatesDevice::displayLine(int x, int y, const std::string &str) {
    impl_->displayLine(x, y, str);
}

//// start of IMPLEMENTATION

// fwd decl for helper functions
extern void cairo_linuxfb_surface_destroy(void *device);
extern cairo_surface_t *cairo_linuxfb_surface_create();
extern int opengpio(const char *pathname, int flags);

FatesDeviceImpl_::FatesDeviceImpl_() {
    ;
}

FatesDeviceImpl_::~FatesDeviceImpl_() {
    ;
}

void FatesDeviceImpl_::addCallback(std::shared_ptr<FatesCallback> cb) {
    callbacks_.push_back(cb);
}

void FatesDeviceImpl_::start() {
    keepRunning_=true;
    initGPIO();
    initDisplay();
}

void FatesDeviceImpl_::stop() {
    keepRunning_=false;

    deinitDisplay();
    deinitGPIO();
}

unsigned FatesDeviceImpl_::process() {
    //encoder/key poll?
    displayPaint();
    return 0;
}


//////  ENCODER AND KEY HANDLING //////////////////////////////////////


//struct EncArg {
//    EncArg(FatesDeviceImpl_* pThis,unsigned enc) : pThis_(pThis),enc_(enc) { ; }
//    FatesDeviceImpl_* pThis_;
//    unsigned enc_;
//};
//
//void *encProcess(void *x) {
//    auto pEncArg= static_cast<EncArg*>(x);
//    auto pThis=pEncArg->pThis_;
//    int n = pEncArg->enc_;
//    delete pEncArg;
//
//    int rd;
//    unsigned int i;
//    struct input_event event[64];
//    int dir[FatesDevice::num_enc] = {1,1,1};
//    clock_t now[3];
//    clock_t prev[3];
//    clock_t diff;
//    prev[0] = prev[1] = prev[2] =  clock();
//
//    while(1) {
//        rd = read(pThis->enc_fd[n], event, sizeof(struct input_event) * 64);
//        if(rd < (int) sizeof(struct input_event)) {
//            fprintf(stderr, "ERROR (enc) read error\n");
//        }
//
//        for(i=0;i<rd/sizeof(struct input_event);i++) {
//            if(event[i].type) { // make sure it's not EV_SYN == 0
//                now[i] = clock();
//                diff = now[i] - prev[i];
//                fprintf(stderr, "%d\t%d\t%lu\n", n, event[i].value, diff);
//                prev[i] = now[i];
//                if(diff > 100) { // filter out glitches
//                    if(dir[i] != event[i].value && diff > 500) { // only reverse direction if there is reasonable settling time
//                        dir[i] = event[i].value;
//                    }
//                }
//            }
//        }
//    }
//}
//
//void *keyProcess(void *x) {
//    auto pThis= static_cast<FatesDeviceImpl_*>(x);
//    int rd;
//    unsigned int i;
//    struct input_event event[64];
//
//    while(1) {
//        rd = read(pThis->key_fd, event, sizeof(struct input_event) * 64);
//        if(rd < (int) sizeof(struct input_event)) {
//            fprintf(stderr, "ERROR (key) read error\n");
//        }
//
//        for(i=0;i<rd/sizeof(struct input_event);i++) {
//            if(event[i].type) { // make sure it's not EV_SYN == 0
//                fprintf(stderr, "enc%d = %d\n", event[i].code, event[i].value);
//            }
//        }
//    }
//}

void* process_gpio_func(void *x) {
    auto pThis= static_cast<FatesDeviceImpl_*>(x);
    pThis->processGPIO();
    return nullptr;
}

#include <sys/poll.h>
void FatesDeviceImpl_::processGPIO() {

    int encdir[FatesDeviceImpl_::NUM_ENCODERS] = {1,1,1};
    clock_t encnow[NUM_ENCODERS];
    clock_t encprev[NUM_ENCODERS];
    encprev[0] = encprev[1] = encprev[2] = encprev[3] =clock();
    struct input_event event[64];

    while (keepRunning_) {

	struct pollfd fds[1+NUM_ENCODERS];
	
	//FIXME only works if order of fd doesnt change
	// i.e key/enc 1 to 3 must open
        unsigned fdcount=0;
	if(keyFd_>0) {
	    fds[fdcount].fd=keyFd_;
	    fds[fdcount].events = POLLIN;
	}
        for(auto i=0;i<NUM_ENCODERS;i++) {
            if(encFd_[i]>0) { 
		    fds[fdcount].fd=encFd_[i];
		    fds[fdcount].events = POLLIN;
		    fdcount++;
	    } else {
	 	fprintf(stderr,"enc %d not opended", i);
	    }
        }

	auto result = poll(fds,fdcount,5000);
	fprintf(stderr,"poll %d %d\n", result, fdcount);
	for(auto c=0;c<fdcount;c++) {
	   fprintf(stderr,"   %d %d %d\n", fds[c].fd, fds[c].revents, fds[c].revents&POLLIN);
	}

        if(result>0) {
            if ( fds[0].revents & POLLIN)  {

                auto rd = read(keyFd_, event, sizeof(struct input_event));
                //auto rd = read(keyFd_, event, sizeof(struct input_event) * 64);
                if(rd < (int) sizeof(struct input_event)) {
                    fprintf(stderr, "ERROR (key) read error\n");
                }
                else {
                    { unsigned i=0;
                    //for (auto i = 0; i < rd / sizeof(struct input_event); i++) {
                        if (event[i].type) { // make sure it's not EV_SYN == 0
                            fprintf(stderr, "button %d = %d\n", event[i].code, event[i].value);
                        }
                    }
                }
            }
        }

        for (auto n=0;n<NUM_ENCODERS;n++) {
            	if ( fds[n+1].revents & POLLIN)  {

                    auto rd = read(encFd_[n], event, sizeof(struct input_event));
                    //auto rd = read(encFd_[n], event, sizeof(struct input_event) * 64);
                    if (rd < (int) sizeof(struct input_event)) {
                        fprintf(stderr, "ERROR (enc) read error\n");
                    }

                    auto now = clock();
                    //for (auto i = 0; i < rd / sizeof(struct input_event); i++) {
                    { unsigned i=0;
                        if (event[i].type) { // make sure it's not EV_SYN == 0
                            auto diff = now - encprev[n];
                            fprintf(stderr, "encoder %d\t%d\t%lu\n", n, event[i].value, diff);
                            encprev[n] = now;
                            if (diff > 100) { // filter out glitches
                                if (encdir[n] != event[i].value && diff > 500) { // only reverse direction if there is reasonable settling time
                                    encdir[n] = event[i].value;
                                }
                            }
                        }
                    }
                }
            }

    }
}


void FatesDeviceImpl_::initGPIO() {

    const char *enc_filenames[NUM_ENCODERS] = {"/dev/input/by-path/platform-soc:knob1-event",
                                    "/dev/input/by-path/platform-soc:knob2-event",
                                    "/dev/input/by-path/platform-soc:knob3-event",
                                    "/dev/input/by-path/platform-soc:knob4-event"};
    for(auto i=0; i < NUM_ENCODERS; i++) {
        encFd_[i] = opengpio(enc_filenames[i], O_RDONLY);
	//encFd_[i]=0;
        if(encFd_[i] > 0) {
		fprintf(stderr, "success open enc %d, %d\n", i,encFd_[i]);
//            EncArg* pEncArg=new EncArg(this,i);
//            if(pthread_create(&enc_p[i], NULL, encProcess, pEncArg) ) {
//                fprintf(stderr, "ERROR (enc%d) pthread error\n",i);
//            }
        }
    }

    keyFd_ = opengpio("/dev/input/by-path/platform-keys-event", O_RDONLY); // Keys
    if(keyFd_ > 0) {
//        if(pthread_create(&key_p, NULL, keyProcess, this) ) {
//            fprintf(stderr, "ERROR (keys) pthread error\n");
//        }
    }

    gpioThread_ = std::thread(process_gpio_func, this);
}

void FatesDeviceImpl_::deinitGPIO() {
    // destroy threads monitor keys and encoders
//    pthread_cancel(key_p);
//    for (unsigned i = 0; i < num_enc; i++) {
//        pthread_cancel(enc_p[i]);
//    }
}

////////////////////////    DISPLAY HANDLING ////////////////////////////////////////////



void FatesDeviceImpl_::initDisplay() {
    surfacefb_ = cairo_linuxfb_surface_create();
    crfb_ = cairo_create(surfacefb_);

    surface_ = cairo_image_surface_create(SCREEN_FMT,SCREEN_X,SCREEN_Y);
    cr_ = cairo_create(surface_);

    cairo_set_operator(crfb_, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(crfb_,surface_,0,0);

    // clear display
    cairo_set_operator(cr_, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr_);
    cairo_set_operator(cr_, CAIRO_OPERATOR_OVER);
    cairo_paint(crfb_);

    //cairo_font_options_t *font_options = cairo_font_options_create();
    //cairo_font_options_set_antialias(font_options,CAIRO_ANTIALIAS_SUBPIXEL);
    //cairo_set_font_options(cr, font_options);
    //cairo_font_options_destroy(font_options);
    cairo_select_font_face(cr_, "serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr_, 8.0);

    cairo_set_source_rgb(cr_, 1, 1,1 ); //!!

}

void FatesDeviceImpl_::deinitDisplay() {
    cairo_destroy(cr_);
    cairo_surface_destroy(surface_);
    cairo_destroy(crfb_);
    cairo_linuxfb_surface_destroy(surfacefb_);
}


void FatesDeviceImpl_::displayPaint() {
    cairo_paint(crfb_);

}

void FatesDeviceImpl_::displayLine(int x , int y , const std::string& str)  {
    cairo_set_source_rgb(cr_, 1, 1,1 ); //!!
    cairo_move_to(cr_,x, y);
    cairo_show_text(cr_, str.c_str());
    cairo_fill(cr_);
}



///////// helper functions

int opengpio (const char *pathname, int flags) {
    int fd;
    int open_attempts=0, ioctl_attempts=0;
    while (open_attempts < 200) {
        fd = open(pathname, flags);
        if(fd > 0) {
            if(ioctl(fd, EVIOCGRAB, 1) == 0) {
                ioctl(fd, EVIOCGRAB, (void*)0);
                goto done;
            }
            ioctl_attempts++;
            close(fd);
        }
        open_attempts++;
        usleep(50000); // 50ms sleep * 200 = 10s fail after 10s
    };
    done:
    if(open_attempts > 0) {
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
void cairo_linuxfb_surface_destroy(void *device)
{
    cairo_linuxfb_device_t *dev = (cairo_linuxfb_device_t *)device;

    if (dev == NULL) {
        return;
    }

    munmap(dev->fb_data, dev->fb_screensize);
    close(dev->fb_fd);
    free(dev);
}

/* Create a cairo surface using the specified framebuffer */
cairo_surface_t *cairo_linuxfb_surface_create()
{
    cairo_linuxfb_device_t *device;
    cairo_surface_t *surface;

    const char* fb_name = FB_DEVICE;

    device = static_cast<cairo_linuxfb_device_t*>( malloc( sizeof(*device) ));
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
    device->fb_data = (unsigned char *)mmap(0, device->fb_screensize,
                                            PROT_READ | PROT_WRITE, MAP_SHARED,
                                            device->fb_fd, 0);

    if ( device->fb_data == (unsigned char *)-1 ) {
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




}// namespace

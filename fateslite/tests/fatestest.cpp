#include "FatesLite.h"
#include <iostream>
#include <unistd.h>
#include <iomanip>
#include <signal.h>

static volatile bool keepRunning = 1;



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



#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include <linux/input.h>
#include <time.h>



int key_fd;
pthread_t key_p;

int enc_fd[3];
pthread_t enc_p[3];

void *key_check(void *);
void *enc_check(void *);

// extern def

static int open_and_grab (const char *pathname, int flags) {
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
        fprintf(stderr, "WARN open_and_grab GPIO '%s' required %d open attempts & %d ioctl attempts\n", pathname, open_attempts, ioctl_attempts);
    }
    return fd;
}

void gpio_init() {
    key_fd = open_and_grab("/dev/input/by-path/platform-keys-event", O_RDONLY); // Keys
    if(key_fd > 0) {
        if(pthread_create(&key_p, NULL, key_check, 0) ) {
            fprintf(stderr, "ERROR (keys) pthread error\n");
        }
    }

    char *enc_filenames[3] = {"/dev/input/by-path/platform-soc:knob1-event",
                              "/dev/input/by-path/platform-soc:knob2-event",
                              "/dev/input/by-path/platform-soc:knob3-event"};
    for(int i=0; i < 3; i++) {
        enc_fd[i] = open_and_grab(enc_filenames[i], O_RDONLY);
        if(enc_fd[i] > 0) {
            int *arg = malloc(sizeof(int));
            *arg = i;
            if(pthread_create(&enc_p[i], NULL, enc_check, arg) ) {
                fprintf(stderr, "ERROR (enc%d) pthread error\n",i);
            }
        }
    }
}

void gpio_deinit() {
    pthread_cancel(key_p);
    pthread_cancel(enc_p[0]);
    pthread_cancel(enc_p[1]);
    pthread_cancel(enc_p[2]);
}

void *enc_check(void *x) {
    int n = *((int *)x);
    free(x);
    int rd;
    unsigned int i;
    struct input_event event[64];
    int dir[3] = {1,1,1};
    clock_t now[3];
    clock_t prev[3];
    clock_t diff;
    prev[0] = prev[1] = prev[2] = clock();

    while(1) {
        rd = read(enc_fd[n], event, sizeof(struct input_event) * 64);
        if(rd < (int) sizeof(struct input_event)) {
            fprintf(stderr, "ERROR (enc) read error\n");
        }

        for(i=0;i<rd/sizeof(struct input_event);i++) {
            if(event[i].type) { // make sure it's not EV_SYN == 0
                now[i] = clock();
                diff = now[i] - prev[i];
                fprintf(stderr, "%d\t%d\t%lu\n", n, event[i].value, diff);
                prev[i] = now[i];
                if(diff > 100) { // filter out glitches
                    if(dir[i] != event[i].value && diff > 500) { // only reverse direction if there is reasonable settling time
                        dir[i] = event[i].value;
                    }
//                    union event_data *ev = event_data_new(EVENT_ENC);
//                    ev->enc.n = n + 1;
//                    ev->enc.delta = event[i].value;
//                    event_post(ev);
                }
            }
        }
    }
}

void *key_check(void *x) {
    (void)x;
    int rd;
    unsigned int i;
    struct input_event event[64];

    while(1) {
        rd = read(key_fd, event, sizeof(struct input_event) * 64);
        if(rd < (int) sizeof(struct input_event)) {
            fprintf(stderr, "ERROR (key) read error\n");
        }

        for(i=0;i<rd/sizeof(struct input_event);i++) {
            if(event[i].type) { // make sure it's not EV_SYN == 0
                fprintf(stderr, "enc%d = %d\n", event[i].code, event[i].value);
//                union event_data *ev = event_data_new(EVENT_KEY);
//                ev->key.n = event[i].code;
//                ev->key.val = event[i].value;
//                event_post(ev);
            }
        }
    }
}




static cairo_t *crmain;
static cairo_t *crfb;
static cairo_surface_t *surface;
static cairo_surface_t *surfacefb;
static cairo_surface_t *image;


#define FB_DEVICE "/dev/fb0"

static cairo_t *cr;
//static cairo_font_face_t *ct[NUM_FONTS];
//static FT_Library value;
//static FT_Error status;
//static FT_Face face[NUM_FONTS];
static double text_xy[2];

#define SCREEN_FB_FMT   CAIRO_FORMAT_ARGB32
#define SCREEN_FMT      CAIRO_FORMAT_ARGB32
#define SCREEN_X 1024
#define SCREEN_Y 600
#define SCREEN_SCALE 8


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
    close(device->fb_fd);
    handle_allocate_error:
    free(device);
    return NULL;
}



FatesLite::FatesDevice device;

void intHandler(int dummy) {
    std::cerr << "FatesTest intHandler called" << std::endl;
    if(!keepRunning) {
        sleep(1);
        exit(-1);
    }
    keepRunning = false;
    device.stop();
}

int main(int argc, const char * argv[]) {
    std::cout << "starting test" << std::endl;
    std::cout << "init gpio" << std::endl;
    gpio_init();
    std::cout << "init screen" << std::endl;
    cairo_surface_t *surface = cairo_linuxfb_surface_create();
    cairo_t *cr = cairo_create(surface);
    signal(SIGINT, intHandler);
    device.start();


    cairo_select_font_face(cr, "serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 16.0);
    cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);


    cairo_set_source_rgb(cr, 0, 0, 0); //!!
    cairo_rectangle(cr,20, 10, 10,24);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 1, 1,1 ); //!!
    cairo_move_to(cr, 20, 10);
    cairo_show_text(cr, "Hello");


    std::cout << "started test" << std::endl;
    while(keepRunning) {
        device.process();
        sleep(1);
    }
    std::cout << "stopping test" << std::endl;
    device.stop();

    std::cout << "deinit gpio" << std::endl;
    gpio_deinit();
    std::cout << "deinit screen" << std::endl;
    cairo_linuxfb_surface_destroy(surface);
    std::cout << "test done" << std::endl;
    return 0;
}

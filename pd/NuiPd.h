#pragma once

#include "m_pd.h"

#include <memory>
#include <NuiDevice.h>

typedef struct _NuiPd {
    t_object x_obj;
    std::shared_ptr<NuiLite::NuiDevice> device_;
} t_NuiPd;


//define pure data methods
extern "C" {
void NuiPd_free(t_NuiPd *);
void *NuiPd_new(t_floatarg);
void NuiPd_setup(void);


// utility
void NuiPd_process(t_NuiPd *x, t_floatarg paint);
void NuiPd_displayClear(t_NuiPd *obj);
void NuiPd_displayPaint(t_NuiPd *obj);
void NuiPd_info(t_NuiPd *obj);


void displayClear();
void displayPaint();

// new api
void NuiPd_gClear(t_NuiPd *obj, t_floatarg clr);
void NuiPd_gSetPixel(t_NuiPd *obj, t_floatarg clr, t_floatarg x, t_floatarg y);
void NuiPd_gFillArea(t_NuiPd *obj, t_floatarg clr, t_floatarg x, t_floatarg y, t_floatarg w, t_floatarg h);
void NuiPd_gCircle(t_NuiPd *obj, t_floatarg clr, t_floatarg x, t_floatarg y, t_floatarg r);
void NuiPd_gFilledCircle(t_NuiPd *obj, t_floatarg clr, t_floatarg x, t_floatarg y, t_floatarg r);
void NuiPd_gLine(t_NuiPd *obj, t_floatarg clr, t_floatarg x1, t_floatarg y1, t_floatarg x2, t_floatarg y2);
void NuiPd_gRectangle(t_NuiPd *obj, t_floatarg clr, t_floatarg x, t_floatarg y, t_floatarg w, t_floatarg h);
void NuiPd_gInvert(t_NuiPd *obj);
void NuiPd_gText(t_NuiPd *obj, t_symbol *s, int argc, t_atom *argv);
void NuiPd_gWaveform(t_NuiPd *obj, t_symbol *s, int argc, t_atom *argv);
void NuiPd_gInvertArea(t_NuiPd *obj, t_floatarg x, t_floatarg y, t_floatarg w, t_floatarg h);
void NuiPd_gPng(t_NuiPd *obj, t_floatarg x, t_floatarg y,  t_symbol *s);
void NuiPd_textLine(t_NuiPd *obj,t_symbol *s, int argc, t_atom *argv);
void NuiPd_invertLine(t_NuiPd *obj, t_floatarg line);
void NuiPd_clearLine(t_NuiPd *obj, t_floatarg clr, t_floatarg line);



// deprecated api
void NuiPd_clearRect(t_NuiPd *obj, t_floatarg x, t_floatarg clr, t_floatarg y, t_floatarg w, t_floatarg h);
void NuiPd_drawText(t_NuiPd *obj, t_symbol *s, int argc, t_atom *argv);
void NuiPd_drawPNG(t_NuiPd *obj, t_floatarg x, t_floatarg y, t_symbol *s);
void NuiPd_displayText(t_NuiPd *obj, t_symbol *s, int argc, t_atom *argv);
void NuiPd_clearText(t_NuiPd *obj, t_floatarg clr, t_floatarg line);
void NuiPd_invertText(t_NuiPd *obj, t_floatarg line);
}

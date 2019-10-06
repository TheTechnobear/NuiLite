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

// graphics
void NuiPd_clearRect(t_NuiPd *obj, t_floatarg x, t_floatarg clr, t_floatarg y, t_floatarg w, t_floatarg h);
void NuiPd_drawText(t_NuiPd *obj, t_symbol *s, int argc, t_atom *argv);
void NuiPd_drawPNG(t_NuiPd *obj, t_floatarg x, t_floatarg y, t_symbol *s);


// text displays
void NuiPd_displayText(t_NuiPd *obj, t_symbol *s, int argc, t_atom *argv);
void NuiPd_clearText(t_NuiPd *obj, t_floatarg clr, t_floatarg line);
void NuiPd_invertText(t_NuiPd obj, t_floatarg line);
}
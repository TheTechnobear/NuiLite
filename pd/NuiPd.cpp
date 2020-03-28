#include "NuiPd.h"

static t_class *NuiPd_class;

// helper methods
void NuiPd_sendMsg(const char *sym, const char *v) {
    t_pd *sendObj = gensym(sym)->s_thing;
    if (!sendObj) {
        post("NuiPd : no send symbol %s", sym);
        return;
    }
    if (v == nullptr) return;
    t_atom args[1];
    SETSYMBOL(&args[0], gensym(v));
    pd_forwardmess(sendObj, 1, args);
}

void NuiPd_sendMsg(const char *sym, t_symbol *v) {
    t_pd *sendObj = gensym(sym)->s_thing;
    if (!sendObj) {
        post("NuiPd : no send symbol %s", sym);
        return;
    }
    if (v == nullptr) return;
    t_atom args[1];
    SETSYMBOL(&args[0], v);
    pd_forwardmess(sendObj, 1, args);
}

void NuiPd_sendMsg(const char *sym, float v) {
    t_pd *sendObj = gensym(sym)->s_thing;
    if (!sendObj) {
        post("NuiPd : no send symbol %s", sym);
        return;
    }
    t_atom args[1];
    SETFLOAT(&args[0], v);
    pd_forwardmess(sendObj, 1, args);
}

class NuiPdCallback : public NuiLite::NuiCallback {
public:
    explicit NuiPdCallback(t_NuiPd *x) : x_(x) {}

    void onButton(unsigned id, unsigned value) override;
    void onEncoder(unsigned id, int value) override;
private:
    t_NuiPd *x_;
};

void NuiPdCallback::onButton(unsigned id, unsigned value) {
    t_pd *sendObj = gensym("nui_button")->s_thing;
    if (!sendObj) {
        post("NuiPd : no send symbol nui_button");
        return;
    }
    t_atom args[2];
    SETFLOAT(&args[0], id);
    SETFLOAT(&args[1], value);
    pd_forwardmess(sendObj, 2, args);
}

void NuiPdCallback::onEncoder(unsigned id, int value) {
    t_pd *sendObj = gensym("nui_encoder")->s_thing;
    if (!sendObj) {
        post("NuiPd : no send symbol nui_encoder");
        return;
    }
    t_atom args[2];
    SETFLOAT(&args[0], id);
    SETFLOAT(&args[1], value);
    pd_forwardmess(sendObj, 2, args);
}


// puredata methods implementation - start

void NuiPd_free(t_NuiPd *x) {
    if (x->device_ != nullptr) x->device_->stop();
    t_pd *nuiStop = gensym("nui_stop")->s_thing;
    if (nuiStop) pd_bang(nuiStop);

    x->device_ = nullptr;
}

void *NuiPd_new(t_floatarg) {
    auto *x = (t_NuiPd *) pd_new(NuiPd_class);
    x->device_ = std::make_shared<NuiLite::NuiDevice>();
    auto cb = std::make_shared<NuiPdCallback>(x);
    x->device_->addCallback(cb);
    x->device_->start();

    t_pd *nuiStart = gensym("nui_start")->s_thing;
    if (nuiStart) pd_bang(nuiStart);

    return (void *) x;
}


void NuiPd_setup(void) {
    NuiPd_class = class_new(gensym("NuiPd"),
                            (t_newmethod) NuiPd_new,
                            (t_method) NuiPd_free,
                            sizeof(t_NuiPd),
                            CLASS_DEFAULT,
                            A_DEFFLOAT, A_NULL);

    class_addmethod(NuiPd_class, (t_method) NuiPd_process, gensym("process"), A_DEFFLOAT, A_NULL);
    class_addmethod(NuiPd_class, (t_method) NuiPd_displayClear, gensym("clear"), A_NULL);
    class_addmethod(NuiPd_class, (t_method) NuiPd_displayPaint, gensym("paint"), A_NULL);
    class_addmethod(NuiPd_class, (t_method) NuiPd_info, gensym("info"), A_NULL);

    // graphics api
    class_addmethod(NuiPd_class, (t_method) NuiPd_gClear, gensym("gClear"), A_DEFFLOAT, A_NULL);
    class_addmethod(NuiPd_class, (t_method) NuiPd_gSetPixel, gensym("gSetPixel"), A_DEFFLOAT, A_DEFFLOAT, A_DEFFLOAT,A_NULL);
    class_addmethod(NuiPd_class, (t_method) NuiPd_gFillArea, gensym("gFillArea"), A_DEFFLOAT, A_DEFFLOAT, A_DEFFLOAT, A_DEFFLOAT, A_DEFFLOAT,A_NULL);
    class_addmethod(NuiPd_class, (t_method) NuiPd_gCircle, gensym("gCircle"), A_DEFFLOAT, A_DEFFLOAT, A_DEFFLOAT,A_DEFFLOAT,A_NULL);
    class_addmethod(NuiPd_class, (t_method) NuiPd_gFilledCircle, gensym("gFilledCircle"), A_DEFFLOAT, A_DEFFLOAT, A_DEFFLOAT,A_DEFFLOAT,A_NULL);
    class_addmethod(NuiPd_class, (t_method) NuiPd_gLine, gensym("gFilledCircle"), A_DEFFLOAT, A_DEFFLOAT, A_DEFFLOAT,A_DEFFLOAT,A_DEFFLOAT,A_NULL);
    class_addmethod(NuiPd_class, (t_method) NuiPd_gRectangle, gensym("gRectangle"), A_DEFFLOAT, A_DEFFLOAT, A_DEFFLOAT,A_DEFFLOAT,A_DEFFLOAT,A_NULL);
    class_addmethod(NuiPd_class, (t_method) NuiPd_gInvert, gensym("gInvert"), A_NULL);
    class_addmethod(NuiPd_class, (t_method) NuiPd_gText, gensym("gText"), A_GIMME, A_NULL);
    class_addmethod(NuiPd_class, (t_method) NuiPd_gWaveform, gensym("gWaveform"), A_GIMME, A_NULL);
    class_addmethod(NuiPd_class, (t_method) NuiPd_gInvertArea, gensym("gInvertArea"), A_DEFFLOAT, A_DEFFLOAT,A_DEFFLOAT,A_DEFFLOAT,A_NULL);
    class_addmethod(NuiPd_class, (t_method) NuiPd_gPng, gensym("gPng"), A_DEFFLOAT, A_DEFFLOAT, A_DEFSYMBOL, A_NULL);
    class_addmethod(NuiPd_class, (t_method) NuiPd_textLine, gensym("textLine"), A_GIMME, A_NULL);
    class_addmethod(NuiPd_class, (t_method) NuiPd_invertLine, gensym("invertLine"), A_DEFFLOAT, A_NULL);
    class_addmethod(NuiPd_class, (t_method) NuiPd_clearLine, gensym("clearLine"), A_DEFFLOAT, A_DEFFLOAT, A_NULL);

    // deprecated api
    class_addmethod(NuiPd_class, (t_method) NuiPd_clearRect, gensym("clearRect"), A_DEFFLOAT, A_DEFFLOAT, A_DEFFLOAT, A_DEFFLOAT, A_DEFFLOAT, A_NULL);
    class_addmethod(NuiPd_class, (t_method) NuiPd_drawText, gensym("drawText"), A_GIMME, A_NULL);
    class_addmethod(NuiPd_class, (t_method) NuiPd_drawPNG, gensym("drawPng"), A_DEFFLOAT, A_DEFFLOAT, A_DEFSYMBOL, A_NULL);
    class_addmethod(NuiPd_class, (t_method) NuiPd_displayText, gensym("displayText"), A_GIMME, A_NULL);
    class_addmethod(NuiPd_class, (t_method) NuiPd_clearText, gensym("clearText"), A_DEFFLOAT, A_DEFFLOAT, A_NULL);
    class_addmethod(NuiPd_class, (t_method) NuiPd_invertText, gensym("invertText"), A_DEFFLOAT, A_NULL);
}


void NuiPd_process(t_NuiPd *obj, t_floatarg paint) {
    if (obj == nullptr || obj->device_ == nullptr) return;
    obj->device_->process(paint > 0);
}

void NuiPd_displayClear(t_NuiPd *obj) {
    if (obj == nullptr || obj->device_ == nullptr) return;
    obj->device_->displayClear();
}

void NuiPd_displayPaint(t_NuiPd *obj) {
    if (obj == nullptr || obj->device_ == nullptr) return;
    obj->device_->displayPaint();
}

void NuiPd_info(t_NuiPd *obj) {
    if (obj == nullptr || obj->device_ == nullptr) return;
    NuiPd_sendMsg("nui_num_buttons", 3);
    NuiPd_sendMsg("nui_num_encoders", (float) obj->device_->numEncoders());
}
/////////////////////////////////////

void NuiPd_gClear(t_NuiPd *obj, t_floatarg clr) {
    if (obj == nullptr || obj->device_ == nullptr) return;
    obj->device_->gClear((unsigned) clr);
}

void NuiPd_gSetPixel(t_NuiPd *obj, t_floatarg clr, t_floatarg x, t_floatarg y) {
    if (obj == nullptr || obj->device_ == nullptr) return;
    obj->device_->gSetPixel((unsigned) clr, (unsigned) x, (unsigned) y);
}

void NuiPd_gFillArea(t_NuiPd *obj, t_floatarg clr, t_floatarg x, t_floatarg y, t_floatarg w, t_floatarg h) {
    if (obj == nullptr || obj->device_ == nullptr) return;
    obj->device_->gFillArea((unsigned) clr, (unsigned) x, (unsigned) y, (unsigned) w, (unsigned) h);
}

void NuiPd_gCircle(t_NuiPd *obj, t_floatarg clr, t_floatarg x, t_floatarg y, t_floatarg r) {
    if (obj == nullptr || obj->device_ == nullptr) return;
    obj->device_->gCircle((unsigned) clr, (unsigned) x, (unsigned) y, (unsigned) r);
}

void NuiPd_gFilledCircle(t_NuiPd *obj, t_floatarg clr, t_floatarg x, t_floatarg y, t_floatarg r) {
    obj->device_->gCircle((unsigned) clr, (unsigned) x, (unsigned) y, (unsigned) r);
}

void NuiPd_gLine(t_NuiPd *obj, t_floatarg clr, t_floatarg x1, t_floatarg y1, t_floatarg x2, t_floatarg y2) {
    obj->device_->gRectangle((unsigned) clr, (unsigned) x1, (unsigned) y1, (unsigned) x2, (unsigned) y2);
}

void NuiPd_gRectangle(t_NuiPd *obj, t_floatarg clr, t_floatarg x, t_floatarg y, t_floatarg w, t_floatarg h) {
    if (obj == nullptr || obj->device_ == nullptr) return;
    obj->device_->gRectangle((unsigned) clr, (unsigned) x, (unsigned) y, (unsigned) w, (unsigned) h);
}

void NuiPd_gInvert(t_NuiPd *obj) {
    if (obj == nullptr || obj->device_ == nullptr) return;
    obj->device_->gInvert();
}

void NuiPd_gText(t_NuiPd *obj, t_symbol *s, int argc, t_atom *argv) {
    if (obj == nullptr || obj->device_ == nullptr) return;
    if (argc < 4) {
        post("error: drawText clr x y string");
    }
    unsigned arg = 0;
    unsigned x = 0, y = 0, clr = 0;
    if (argv[arg].a_type == A_FLOAT) {
        clr = atom_getfloat(&argv[arg]);
    }
    arg++;
    if (argv[arg].a_type == A_FLOAT) {
        x = atom_getfloat(&argv[arg]);
    }
    arg++;
    if (argv[arg].a_type == A_FLOAT) {
        y = atom_getfloat(&argv[arg]);
    }
    arg++;


    std::string str;
    for (unsigned i = arg; i < argc; i++) {
        t_atom *targ = argv + i;
        if (targ->a_type == A_SYMBOL) {
            t_symbol *sym = atom_getsymbol(targ);
            str += sym->s_name;
        } else if (targ->a_type == A_FLOAT) {
            std::string fstr = std::to_string(atom_getfloat(targ));
            fstr.erase(fstr.find_last_not_of('0') + 1, std::string::npos);
            fstr.erase(fstr.find_last_not_of('.') + 1, std::string::npos);
            str += fstr;
        }

        if (i != argc - 1) str += " ";
    }
    obj->device_->gText(clr, x, y, str);
}

void NuiPd_gWaveform(t_NuiPd *obj,  t_symbol *s, int argc, t_atom *argv) {
    unsigned arg = 0;
    unsigned clr = 0;
    if (argv[arg].a_type == A_FLOAT) {
        clr = atom_getfloat(&argv[arg]);
    }

    arg++;
    std::vector<unsigned> wave;
    for (unsigned i = arg; i < argc; i++) {
        t_atom *targ = argv + i;
        if (targ->a_type == A_FLOAT) {
            wave.push_back((unsigned) atom_getfloat(targ));
        }
    }

    obj->device_->gWaveform(clr,wave);
}

void NuiPd_gInvertArea(t_NuiPd *obj, t_floatarg x, t_floatarg y, t_floatarg w, t_floatarg h) {
    if (obj == nullptr || obj->device_ == nullptr) return;
    obj->device_->gInvertArea( (unsigned) x, (unsigned) y, (unsigned) w, (unsigned) h);
}

void NuiPd_gPng(t_NuiPd *obj, t_floatarg x, t_floatarg y, t_symbol *s) {
    if (obj == nullptr || obj->device_ == nullptr || s == nullptr || s->s_name == nullptr) return;
    obj->device_->gPng((unsigned) x, (unsigned) y, s->s_name);
}


void NuiPd_textLine(t_NuiPd *obj, t_symbol *, int argc, t_atom *argv) {
    if (obj == nullptr || obj->device_ == nullptr) return;
    if (argc < 4) {
        post("error: NuiPd_textLine clr line col string");
    }
    unsigned arg = 0;
    unsigned line = 0, col = 0, clr = 0;
    if (argv[arg].a_type == A_FLOAT) {
        clr = atom_getfloat(&argv[arg]);
    }
    arg++;
    if (argv[arg].a_type == A_FLOAT) {
        line = atom_getfloat(&argv[arg]);
    }
    arg++;
    if (argv[arg].a_type == A_FLOAT) {
        col = atom_getfloat(&argv[arg]);
    }
    arg++;


    std::string str;
    for (unsigned i = arg; i < argc; i++) {
        t_atom *targ = argv + i;
        if (targ->a_type == A_SYMBOL) {
            t_symbol *sym = atom_getsymbol(targ);
            str += sym->s_name;
        } else if (targ->a_type == A_FLOAT) {
            std::string fstr = std::to_string(atom_getfloat(targ));
            fstr.erase(fstr.find_last_not_of('0') + 1, std::string::npos);
            fstr.erase(fstr.find_last_not_of('.') + 1, std::string::npos);
            str += fstr;
        }

        if (i != argc - 1) str += " ";
    }
    obj->device_->textLine(clr, line, col, str);
}

void NuiPd_invertLine(t_NuiPd *obj, t_floatarg line) {
    if (obj == nullptr || obj->device_ == nullptr) return;
    obj->device_->invertLine((unsigned) line);
}

void NuiPd_clearLine(t_NuiPd *obj, t_floatarg clr, t_floatarg line) {
    if (obj == nullptr || obj->device_ == nullptr) return;
    obj->device_->clearLine((unsigned) clr, (unsigned) line);
}

// deprecated api
void NuiPd_clearRect(t_NuiPd *obj, t_floatarg clr, t_floatarg x, t_floatarg y, t_floatarg w, t_floatarg h) {
    if (obj == nullptr || obj->device_ == nullptr) return;
    NuiPd_gFillArea(obj, clr, x, y, w, h);
}

void NuiPd_drawText(t_NuiPd *obj, t_symbol *s, int argc, t_atom *argv) {
    NuiPd_gText(obj,s,argc,argv);
}

void NuiPd_drawPNG(t_NuiPd *obj, t_floatarg x, t_floatarg y, t_symbol *s) {
    NuiPd_gPng(obj,x,y,s);
}
void NuiPd_displayText(t_NuiPd *obj, t_symbol *s, int argc, t_atom *argv) {
    NuiPd_textLine(obj,s,argc,argv);
}
void NuiPd_clearText(t_NuiPd *obj, t_floatarg clr, t_floatarg line) {
    NuiPd_clearLine(obj,clr,line);
}
void NuiPd_invertText(t_NuiPd *obj, t_floatarg line) {
    NuiPd_invertLine(obj,line);
}




// puredata methods implementation - end


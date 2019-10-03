#include "SKCallback.h"
#include "SKApp.h"

SKCallback::SKCallback(SKApp &app) : app_(app) {

}

void SKCallback::init() {
}

void SKCallback::onButton(unsigned id, unsigned value) {
    app_.onButton(id, value);
}

void SKCallback::onEncoder(unsigned id, int value) {
    app_.onEncoder(id, value);
}

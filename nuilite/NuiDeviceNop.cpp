#include "NuiDevice.h"


namespace NuiLite {

NuiDevice::NuiDevice(const char *) {
    throw std::runtime_error("Unsupported platform");
}

NuiDevice::~NuiDevice() {
}

void NuiDevice::start() {
}

void NuiDevice::stop() {
}

unsigned NuiDevice::process(bool) {
    return 0;
}

void NuiDevice::addCallback(std::shared_ptr<NuiCallback>) {
}

bool NuiDevice::buttonState(unsigned) {
    return false;
}

unsigned NuiDevice::numEncoders() {
    return 0;
}


void NuiDevice::displayClear() {
}

void NuiDevice::displayPaint() {
}


void NuiDevice::gClear(unsigned) {
}


void NuiDevice::gSetPixel(unsigned, unsigned, unsigned) {
}


void NuiDevice::gFillArea(unsigned, unsigned, unsigned, unsigned, unsigned) {
}


void NuiDevice::gCircle(unsigned, unsigned, unsigned, unsigned) {
}


void NuiDevice::gFilledCircle(unsigned, unsigned, unsigned, unsigned) {
}


void NuiDevice::gLine(unsigned, unsigned, unsigned, unsigned, unsigned) {
}


void NuiDevice::gRectangle(unsigned, unsigned, unsigned, unsigned, unsigned) {
}


void NuiDevice::gInvert() {
}

void NuiDevice::gText(unsigned, unsigned, unsigned, const std::string &) {
}

void NuiDevice::gWaveform(unsigned, const std::vector<unsigned> &) {
}

void NuiDevice::gInvertArea(unsigned, unsigned, unsigned, unsigned) {
}

void NuiDevice::gPng(unsigned, unsigned, const char *) {
}

void NuiDevice::textLine(unsigned, unsigned, unsigned, const std::string &) {
}

void NuiDevice::clearLine(unsigned, unsigned) {
}

void NuiDevice::invertLine(unsigned) {
}


// old api, for backwards compat only - depreciated! 
void NuiDevice::drawPNG(unsigned, unsigned, const char *) {
}

void NuiDevice::clearRect(unsigned, unsigned, unsigned, unsigned, unsigned) {
}

void NuiDevice::drawText(unsigned, unsigned, unsigned, const std::string &) {
}

void NuiDevice::displayText(unsigned, unsigned, unsigned, const std::string &) {
}

void NuiDevice::invertText(unsigned) {
}

void NuiDevice::clearText(unsigned, unsigned) {
}


}// namespace

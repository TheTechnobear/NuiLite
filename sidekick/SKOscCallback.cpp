#include "SKOscCallback.h"
#include "SKApp.h"

#include <iostream>

SKOscCallback::SKOscCallback(SKApp &app) : app_(app) {

}

unsigned getUnsignedArg(osc::ReceivedMessage::const_iterator &arg) {
    if (arg->IsInt32()) {
        return (unsigned) (arg++)->AsInt32();
    } else if (arg->IsFloat()) {
        return (unsigned) (arg++)->AsFloat();
    }
    return 0;
}

void SKOscCallback::ProcessMessage(const osc::ReceivedMessage &m,
                                   const IpEndpointName &remoteEndpoint) {
    (void) remoteEndpoint; // suppress unused parameter warning

    try {
        osc::ReceivedMessageArgumentStream args = m.ArgumentStream();
        std::string addr = m.AddressPattern();
        if (addr == "/nui/stopSidekick") {
            app_.stopPatch();
        } else if (addr == "/nui/displayClear") {
            app_.device().displayClear();
        } else if (addr == "/nui/displayPain") {
            app_.device().displayPaint();
        } else if (addr == "/nui/gClear") {
            osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
            unsigned clr = getUnsignedArg(arg);
            app_.device().gClear(clr);
        } else if (addr == "/nui/gSetPixel") {
            osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
            unsigned clr = getUnsignedArg(arg);
            unsigned x = getUnsignedArg(arg);
            unsigned y = getUnsignedArg(arg);
            app_.device().gSetPixel(clr, x, y);
        } else if (addr == "/nui/gFillArea") {
            osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
            unsigned clr = getUnsignedArg(arg);
            unsigned x = getUnsignedArg(arg);
            unsigned y = getUnsignedArg(arg);
            unsigned w = getUnsignedArg(arg);
            unsigned h = getUnsignedArg(arg);
            app_.device().gFillArea(clr, x, y, w, h);
        } else if (addr == "/nui/gCircle") {
            osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
            unsigned clr = getUnsignedArg(arg);
            unsigned x = getUnsignedArg(arg);
            unsigned y = getUnsignedArg(arg);
            unsigned r = getUnsignedArg(arg);
            app_.device().gCircle(clr, x, y, r);
        } else if (addr == "/nui/gFilledCircle") {
            osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
            unsigned clr = getUnsignedArg(arg);
            unsigned x = getUnsignedArg(arg);
            unsigned y = getUnsignedArg(arg);
            unsigned r = getUnsignedArg(arg);
            app_.device().gFilledCircle(clr, x, y, r);
        } else if (addr == "/nui/gLine") {
            osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
            unsigned clr = getUnsignedArg(arg);
            unsigned x1 = getUnsignedArg(arg);
            unsigned y1 = getUnsignedArg(arg);
            unsigned x2 = getUnsignedArg(arg);
            unsigned y2 = getUnsignedArg(arg);
            app_.device().gLine(clr, x1, y1, x2, y2);
        } else if (addr == "/nui/gRectangle") {
            osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
            unsigned clr = getUnsignedArg(arg);
            unsigned x = getUnsignedArg(arg);
            unsigned y = getUnsignedArg(arg);
            unsigned w = getUnsignedArg(arg);
            unsigned h = getUnsignedArg(arg);
            app_.device().gRectangle(clr, x, y, w, h);
        } else if (addr == "/nui/gInvert") {
            app_.device().gInvert();
        } else if (addr == "/nui/gText") {
            osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
            unsigned clr = getUnsignedArg(arg);
            unsigned x = getUnsignedArg(arg);
            unsigned y = getUnsignedArg(arg);
            const char *str = (arg++)->AsString();
            app_.device().gText(clr, x, y, str);
        } else if (addr == "/nui/gWaveform") {
            std::vector<unsigned> wave;
            osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
            unsigned clr = getUnsignedArg(arg);
            if (arg->IsBlob()) {
                const void *blob;
                osc::osc_bundle_element_size_t size;
                (arg++)->AsBlob( blob, size);
                for (int i = 0; i < size; i++) {
                    const uint8_t* values=static_cast<const uint8_t *>(blob);
                    wave.push_back(values[i]);
                }
            }
            app_.device().gWaveform(clr, wave); // 128 values, of 0..64
        } else if (addr == "/nui/gInvertArea") {
            osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
            unsigned x = getUnsignedArg(arg);
            unsigned y = getUnsignedArg(arg);
            unsigned w = getUnsignedArg(arg);
            unsigned h = getUnsignedArg(arg);
            app_.device().gInvertArea(x, y, w, h);
        } else if (addr == "/nui/gPng") {
            osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
            unsigned x = getUnsignedArg(arg);
            unsigned y = getUnsignedArg(arg);
            const char *filename = (arg++)->AsString();
            app_.device().gPng(x, y, filename);
        } else if (addr == "/nui/textLine") {
            osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
            unsigned clr = getUnsignedArg(arg);
            unsigned line = getUnsignedArg(arg);
            unsigned col = getUnsignedArg(arg);
            const char *str = (arg++)->AsString();
            app_.device().textLine(clr, line, col, str);
        } else if (addr == "/nui/invertLine") {
            osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
            unsigned line = getUnsignedArg(arg);
            app_.device().invertLine(line);
        } else if (addr == "/nui/clearLine") {
            osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
            unsigned clr = getUnsignedArg(arg);
            unsigned line = getUnsignedArg(arg);
            app_.device().clearLine(clr, line);
        } else {
            std::cerr << "unrecognied msg addr" << addr << std::endl;
        }


    } catch (osc::Exception &e) {
        // any parsing errors such as unexpected argument types, or
        // missing arguments get thrown as exceptions.
        std::cerr << "error while parsing message: " << m.AddressPattern() << ": " << e.what();
    }
}
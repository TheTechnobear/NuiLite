#pragma once

#include "SKApp.h"
#include <readerwriterqueue.h>
#include <osc/OscPacketListener.h>

class SKApp;


// this simply puts the osc message on a queue, to be handled in the main thread.
class SKOscPacketListener : public PacketListener {
public:
    explicit SKOscPacketListener(moodycamel::ReaderWriterQueue<SKApp::OscMsg> &queue) : queue_(queue) {
    }

    virtual void ProcessPacket(const char *data, int size,
                               const IpEndpointName &remoteEndpoint) {

        SKApp::OscMsg msg;
//        msg.origin_ = remoteEndpoint;
        msg.size_ = (size > SKApp::OscMsg::MAX_OSC_MESSAGE_SIZE ? SKApp::OscMsg::MAX_OSC_MESSAGE_SIZE : size);
        memcpy(msg.buffer_, data, (size_t) msg.size_);
        msg.origin_ = remoteEndpoint;
        queue_.enqueue(msg);
    }

private:
    moodycamel::ReaderWriterQueue<SKApp::OscMsg> &queue_;
};


// this is used within main thread to process the osc inbound messages
class SKOscCallback : public osc::OscPacketListener {
public:
    explicit SKOscCallback(SKApp &app);
    void ProcessMessage(const osc::ReceivedMessage &m, const IpEndpointName &remoteEndpoint) override;
private:
    SKApp &app_;
};


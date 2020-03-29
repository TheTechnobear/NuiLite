#pragma once


#include <string>
#include <memory>
#include <vector>
#include <thread>


#include <ip/UdpSocket.h>
#include <readerwriterqueue.h>

#include "NuiDevice.h"

class SKOscPacketListener;

class SKOscCallback;

class SKPrefs;

class SKApp {
public:
    SKApp();

    void init(SKPrefs &);
    void run();
    void stop();

    void onButton(unsigned id, unsigned value);
    void onEncoder(unsigned id, int value);

    void processOsc(); // dont call!
    void processOscWrite(); // dont call!
private:

    friend class SKOscPacketListener;

    friend class SKOscCallback;

    struct MenuItem {
        enum Type {
            ShellPatch,
            PdPatch,
            ScPatch,
            InstallFile,
            System,
            Dir,
            RefreshMenu,
            RefreshSystem,
            PowerOff,
            MAX_ITEMS
        } type_;

        explicit MenuItem(const std::string &n, MenuItem::Type t, bool s) : name_(n), type_(t), system_(s) { ; }

        std::string name_;
        bool system_;
    };

    static int execShell(const std::string &cmd);
    static void runScript(const std::string &root, const std::shared_ptr<MenuItem> &item, const std::string &cmd);
    void runPd(const std::string &root, const std::shared_ptr<MenuItem> &item);
    void runSc(const std::string &root, const std::shared_ptr<MenuItem> &item);
    void runInstall(const std::string &root, const std::shared_ptr<MenuItem> &item);
    void runRefreshMenu();
    void runRefreshSystem();
    void runPowerOff();
    void stopPatch();
    void runDir(const std::string &root, const std::shared_ptr<MenuItem> &item);

    NuiLite::NuiDevice &device() { return device_; }


    static int checkFileExists(const std::string &filename);
    void displayMenu();
    void activateItem();
    void saveState(const std::shared_ptr<MenuItem> &item);
    void reloadMenu();
    void loadMenu(const std::string &dir, bool sys);
    static std::string getCmdOptions(const std::string &file);

    void runUpdate(const std::string &root);

    void startOscServer();
    void sendOsc(const char *data, unsigned size);
    void sendSKOscEvent(const std::string &event, const std::string data = "");
    void sendDeviceInfo();

    struct OscMsg {
        static const int MAX_N_OSC_MSGS = 64;
        static const int MAX_OSC_MESSAGE_SIZE = 128;
        int size_;
        char buffer_[MAX_OSC_MESSAGE_SIZE];
        IpEndpointName origin_; // only used when for recv
    };

    std::string getFile(const std::string& topDir,const std::string& dir, const std::string& file);

    unsigned POLL_MS_ = 1;
    unsigned ACTIVE_TIME_ = 5000; //5000 mSec
    std::string topPatchDir_;
    std::string patchDir_;
    std::string topSystemDir_;
    std::string systemDir_;

    std::string     lastActiveDir_;
    MenuItem::Type  lastActiveType_;
    int activeCount_ = -1;

    NuiLite::NuiDevice device_;


    std::vector<std::shared_ptr<MenuItem>> mainMenu_;

    bool sidekickActive_ = false;
    unsigned selIdx_ = 0;
    unsigned menuOffset_ = 0;
    bool buttonState_[4] = {false, false, false, false};
    unsigned maxItems_ = 6;
    bool keepRunning_;

    bool loadOnStartup_;
    std::string stateFile_;
    std::string pdOpts_;
    std::string scOpts_;

    // listen for osc
    unsigned listenPort_ = 3001;
    std::thread osc_server_;
    std::unique_ptr<UdpListeningReceiveSocket> oscListenSocket_;
    std::shared_ptr<SKOscPacketListener> oscListener_;
    std::shared_ptr<SKOscCallback> oscProcessor_;
    moodycamel::ReaderWriterQueue<OscMsg> readMessageQueue_;

    // send osc
    unsigned sendPort_ = 3000;
    std::string sendAddr_= "127.0.0.1";
    std::thread osc_writer_;
    std::shared_ptr<UdpTransmitSocket> oscWriteSocket_;
    moodycamel::BlockingReaderWriterQueue<OscMsg> writeMessageQueue_;

    static constexpr unsigned OSC_OUTPUT_BUFFER_SIZE = 1024;
    char osc_write_buffer_[OSC_OUTPUT_BUFFER_SIZE]{};
};

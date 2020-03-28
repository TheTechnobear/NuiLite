#include "SKApp.h"
#include "SKHardwareCallback.h"
#include "SKOscCallback.h"
#include "SKPrefs.h"

#include "NuiDevice.h"
#include <unistd.h>

#include <dirent.h>
#include <sys/stat.h>

#include <iostream>
#include <iomanip>
#include <clocale>

// osc
#include <osc/OscOutboundPacketStream.h>
#include <osc/OscReceivedElements.h>
#include <osc/OscPacketListener.h>
#include <ip/UdpSocket.h>


// for saving state
#include <fstream>
#include <cJSON.h>

SKApp::SKApp() :
    sidekickActive_(false),
    selIdx_(0),
    loadOnStartup_(false),
    keepRunning_(true),
    writeMessageQueue_(OscMsg::MAX_N_OSC_MSGS),
    readMessageQueue_(OscMsg::MAX_N_OSC_MSGS) {

}


void SKApp::init(SKPrefs &prefs) {
    auto cb = std::make_shared<SKHardwareCallback>(*this);
    selIdx_ = 0;
    menuOffset_ = 0;
    sidekickActive_ = false;
    keepRunning_ = true;
    activeCount_ = -1;

    // load preferences
    patchDir_ = prefs.getString("patchDir", "./patches");
    systemDir_ = prefs.getString("SystemDir", "./system");
    ACTIVE_TIME_ = (unsigned) prefs.getInt("activeTime", 5000);
    POLL_MS_ = (unsigned) prefs.getInt("pollTime", 1);
    loadOnStartup_ = prefs.getBool("loadOnStartup", true);
    stateFile_ = prefs.getString("stateFile", "./sidekick-state.json");
    pdOpts_ = prefs.getString("pdOpts", "-nogui -rt -audiobuf 4 -alsamidi");

    listenPort_ = (unsigned) prefs.getInt("listenPort", 4000);
    sendPort_ = (unsigned) prefs.getInt("sendPort", 4001);

    bool menuOnStartup = prefs.getBool("menuOnStartup", false);
    std::setlocale(LC_ALL, "en_US.UTF-8");
    reloadMenu();

    cb->init();

    device_.addCallback(cb);
    device_.start();

    SKPrefs stat(stateFile_);
    if (stat.valid()) {
        lastPatch_ = stat.getString("lastPatch", "");
    }

    bool lastPatchFound = false;
    unsigned idx = 0;
    for (const auto &mi:mainMenu_) {
        if (mi->name_ == lastPatch_) {
            selIdx_ = idx;
            lastPatchFound = true;
            break;
        }
        idx++;
    }

    if (menuOnStartup) {
        activeCount_ = 1;
    } else if (loadOnStartup_) {
        if (device_.buttonState(0)) {
            std::cerr << "button 1 during startup : force menu" << std::endl;
            activeCount_ = 1;
        } else {
            if (!lastPatchFound) {
                std::cerr << "last patch invalid, bring up menu" << std::endl;
                activeCount_ = 1;
            } else {
                std::cerr << "load startup patch: " << lastPatch_ << std::endl;
                activateItem();
            }
        }
    }
    startOscServer();
}

void SKApp::stop() {
    sendOscEvent("sidekickStop");
    //std::cerr << "SKApp::stop" << std::endl;
    keepRunning_ = false;
    oscListenSocket_->AsynchronousBreak();
    //std::cerr << "SKApp::run end" << std::endl;
    if (osc_server_.joinable()) {
        osc_server_.join();
    }
    oscListenSocket_.reset();

    if (osc_writer_.joinable()) {
        osc_writer_.join();
    }
    oscWriteSocket_.reset();
    device_.stop();
}


void SKApp::run() {
    while (keepRunning_) {
        device_.process(sidekickActive_);
        OscMsg msg;
        bool paint = false;
        while (readMessageQueue_.try_dequeue(msg)) {
            oscProcessor_->ProcessPacket(msg.buffer_, msg.size_, msg.origin_);
            paint = true;
        }
        if (paint) device_.displayPaint();
        if (activeCount_ > 0) {
            activeCount_--;
            if (activeCount_ <= 0) {
                activeCount_ = -1;
                sidekickActive_ = true;
                if (sidekickActive_) {
                    std::cerr << "Sidekick activated,stop processes" << std::endl;
                    stopPatch();
                }
            }
        }
        usleep(POLL_MS_ * 1000);
    }
    stop();
}

void SKApp::stopPatch() {
    sendOscEvent("sidekickStopPatch");
    for (const auto &mi:mainMenu_) {
        std::string root;
        if (mi->type_ == MenuItem::System) {
            root = systemDir_;
        } else {
            root = patchDir_;
        }
        std::string stopfile = root + "/" + mi->name_ + "/stop.sh";
        if (checkFileExists(stopfile)) {
            std::string shcmd = "cd \"" + root + "/" + mi->name_ + "\"; ./stop.sh";
            execShell(shcmd);
        } else if (mi->type_ == MenuItem::PdPatch) {
            execShell("killall pd &");
        }
    }
    displayMenu();
    device_.displayPaint();
    sidekickActive_ = true;
}

int SKApp::execShell(const std::string &cmd) {
    std::cout << "exec : " << cmd << std::endl;
    return system(cmd.c_str());
}


void SKApp::runScript(const std::string &root, const std::string &name, const std::string &cmd) {
    std::string shcmd = "cd \"" + root + "/" + name + "\"; ./" + cmd + " &";
    execShell(shcmd);
}


void SKApp::runPd(const std::string &root, const std::string &name) {
    std::string pdOpts = pdOpts_;
    int nogui = execShell("xprop -root > /dev/null");
    if (!nogui) {
        // overides existing options
        pdOpts = pdOpts + " -gui -audiobuf 10";
    }

    std::string pdOptsFile;
    if (checkFileExists(root + "/" + name + "/pd-opts.txt")) pdOptsFile = root + "/" + name + "/pd-opts.txt";
    else if (checkFileExists("./pd-opts.txt")) pdOptsFile = "./pd-opts.txt";
    if (pdOptsFile.length() > 0) {
        pdOpts = pdOpts + " " + getCmdOptions(pdOptsFile);
        std::cout << "using cmd options file : " << pdOptsFile << " options : " << pdOpts << std::endl;
    }

    std::string shcmd = "cd \"" + root + "/" + name + "\"; pd " + pdOpts + " main.pd &";
    std::cout << "pd running : " << shcmd << std::endl;
    execShell(shcmd);
}


void SKApp::runInstall(const std::string &root, const std::string &name) {
    std::string dir = name.substr(0, name.length() - 4);
    std::string shcmd = "cd \"" + root + "\"; unzip \"" + name + "\"; rm \"" + name + "\"; cd \"" + dir + "\"; ./deploy.sh";
    std::cout << "script running : " << shcmd << std::endl;
    execShell(shcmd);
    // reload menu, since new items will appear
    sidekickActive_ = true;
    reloadMenu();
    displayMenu();
}

void SKApp::runRefreshMenu() {
    std::cout << "refresh menu" << std::endl;
    sidekickActive_ = true;
    reloadMenu();
    displayMenu();
}

void SKApp::runRefreshSystem() {
    std::cout << "refresh system" << std::endl;
    sidekickActive_ = true;
    // just in case, something has been manually added!
    reloadMenu();

    device_.displayClear();
    device_.displayText(15, 0, 0, "Checking for updates ...");
    device_.displayText(15, 2, 0, "Updating repo...");
    device_.displayPaint();

    execShell("sudo apt update");

    // now look for update files
    for (const auto &mi:mainMenu_) {
        std::string root;
        if (mi->type_ == MenuItem::System) {
            root = systemDir_;
        } else {
            root = patchDir_;
        }
        std::string updatefile = root + "/" + mi->name_ + "/update.sh";
        if (checkFileExists(updatefile)) {
            std::string txt = "Checking " + mi->name_ + " ...";
            device_.clearText(0, 2);
            device_.displayText(15, 2, 0, txt);
            device_.displayPaint();
            std::string shcmd = "cd \"" + root + "/" + mi->name_ + "\"; ./update.sh";
            std::cout << "update running : " << shcmd << std::endl;
            execShell(shcmd);
        }
    }

    // update sidekick afterwards, since it may restart sidekick
    device_.clearText(0, 2);
    device_.displayText(15, 2, 0, "Checking sidekick...");
    device_.displayPaint();
    execShell("sudo apt install -y sidekick");
    device_.clearText(0, 2);
    device_.displayText(15, 2, 0, "Completed");
    device_.displayPaint();
    sleep(1);

    displayMenu();
}

void SKApp::runPowerOff() {
    device_.displayClear();
    device_.displayText(15, 0, 0, "Powering Down...");
    device_.displayPaint();
    execShell("sudo poweroff");
}


int SKApp::checkFileExists(const std::string &filename) {
    struct stat st{};
    int result = stat(filename.c_str(), &st);
    return result == 0;
}

void SKApp::displayMenu() {
    device_.displayClear();
    unsigned idx = 0;
    unsigned line = 0;
    auto mi = mainMenu_.begin();
    for (int i = 0; i < menuOffset_ && mi != mainMenu_.end(); i++) {
        mi++;
        idx++;
    }

    for (; mi != mainMenu_.end() && line < maxItems_; mi++) {
        std::string txt = (*mi)->name_;
        if ((*mi)->type_ == MenuItem::InstallFile) txt = "Install " + txt;
        device_.displayText(15, line, 0, txt);
        if (idx == selIdx_) device_.invertText(line);
        line++;
        idx++;
    }
}

void SKApp::activateItem() {
    try {
        auto item = mainMenu_.at(selIdx_);
        device_.displayClear();
        device_.displayText(15, 0, 0, "Launch...");
        device_.displayText(15, 1, 0, item->name_);
        std::cerr << "launch : " << item->name_ << std::endl;
        sendOscEvent("sidekickLaunchItem");
        device_.displayPaint();
        sidekickActive_ = false;
        switch (item->type_) {
            case MenuItem::System: {
                runScript(systemDir_, item->name_, "run.sh");
                break;
            }
            case MenuItem::PdPatch:
            case MenuItem::ShellPatch: {

                lastPatch_ = item->name_;
                if (loadOnStartup_ && stateFile_.length()) {
                    saveState();
                }

                std::string prePatch;
                if (checkFileExists(patchDir_ + "/" + item->name_ + "/pre-patch.sh")) prePatch = patchDir_ + "/" + item->name_ + "/pre-patch.sh";
                else if (checkFileExists("./pre-patch.sh")) prePatch = "./pre-patch.sh";
                if (prePatch.length() > 0) execShell("\"" + prePatch + "\"");

                if (item->type_ == MenuItem::PdPatch) {
                    runPd(patchDir_, item->name_);
                } else {
                    runScript(patchDir_, item->name_, "run.sh");
                }

                std::string postPatch;
                if (checkFileExists(patchDir_ + "/" + item->name_ + "/post-patch.sh")) postPatch = patchDir_ + "/" + item->name_ + "/post-patch.sh";
                else if (checkFileExists("./post-patch.sh")) postPatch = "./post-patch.sh";
                if (postPatch.length() > 0) execShell("\"" + postPatch + "\"");

                break;
            }
            case MenuItem::InstallFile : {
                runInstall(patchDir_, item->name_);
                break;
            }
            case MenuItem::RefreshMenu : {
                runRefreshMenu();
                break;
            }
            case MenuItem::RefreshSystem : {
                runRefreshSystem();
                break;
            }
            case MenuItem::PowerOff : {
                runPowerOff();
                break;
            }
            default:
                break;
        }
    } catch (std::out_of_range &) {

    }
}

void SKApp::saveState() {
    if (stateFile_.length() == 0) return;

    std::ofstream outfile(stateFile_.c_str());
    cJSON *root = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "lastPatch", lastPatch_.c_str());

    // const char* text = cJSON_PrintUnformatted(root);
    const char *text = cJSON_Print(root);
    outfile << text << std::endl;
    outfile.close();
    cJSON_Delete(root);
}

void SKApp::reloadMenu() {
    menuOffset_ = 0;
    selIdx_ = 0;
    mainMenu_.clear();
    loadMenu(patchDir_, false);
    loadMenu(systemDir_, true);
    mainMenu_.push_back(std::make_shared<MenuItem>("Check for Updates", MenuItem::RefreshSystem));
    mainMenu_.push_back(std::make_shared<MenuItem>("Refresh Menu", MenuItem::RefreshMenu));
    mainMenu_.push_back(std::make_shared<MenuItem>("Power OFF ", MenuItem::PowerOff));
}


void SKApp::loadMenu(const std::string &dir, bool sys) {
    struct dirent **namelist;
    int n = scandir(dir.c_str(), &namelist, nullptr, alphasort);

    for (int i = 0; i < n; i++) {
        char *fname = namelist[i]->d_name;
        if (fname[0] != '.') {
            switch (namelist[i]->d_type) {
                case DT_DIR : {
                    std::string patchlocation = dir + "/" + fname;
                    std::string mainpd = patchlocation + "/main.pd";
                    std::string shellfile = patchlocation + "/run.sh";
                    bool pd = checkFileExists(mainpd);
                    bool sh = checkFileExists(shellfile);
                    if (pd) {
                        auto menuItem = std::make_shared<MenuItem>(fname, MenuItem::PdPatch);
                        mainMenu_.push_back(menuItem);
                    } else if (sh) {
                        auto menuItem = std::make_shared<MenuItem>(fname, sys ? MenuItem::System : MenuItem::ShellPatch);
                        mainMenu_.push_back(menuItem);
                    }
                    break;
                } //DT_DIR
                case DT_REG: {
                    // zip file for installation
                    int len = strlen(fname);
                    if (len > 4) {
                        char ext[5];
                        ext[0] = fname[len - 4];
                        ext[4] = 0;
                        for (auto ci = 1; ci < 4; ci++) {
                            ext[ci] = std::toupper(fname[len - 4 + ci]);
                        }
                        if (strcmp(ext, ".ZIP") == 0) {
                            auto menuItem = std::make_shared<MenuItem>(fname, MenuItem::InstallFile);
                            mainMenu_.push_back(menuItem);
                        }
                    }
                    break;
                } //DT_REG
                default:
                    break;

            }// switch
        }
        free(namelist[i]);
    }
    free(namelist);
}


void SKApp::onButton(unsigned id, unsigned value) {
    buttonState_[id] = (bool) value;
    if (!sidekickActive_) {
        bool all3 = true;
        for (int i = 0; i < 3; i++) {
            all3 = all3 & buttonState_[i];
        }

        if (all3) {
            activeCount_ = (int) ACTIVE_TIME_;
        } else {
            activeCount_ = -1;
        }
    } else if (id == 0 && value) {
        std::cerr << "Sidekick launch " << std::endl;
        activateItem();
    }

    osc::OutboundPacketStream ops(osc_write_buffer_, OSC_OUTPUT_BUFFER_SIZE);
    ops << osc::BeginBundleImmediate
        << osc::BeginMessage("/nui/button")
        << (int32_t) id
        << (int32_t) value
        << osc::EndMessage
        << osc::EndBundle;
    sendOsc(ops.Data(), ops.Size());
}

void SKApp::onEncoder(unsigned id, int value) {
    if (!sidekickActive_) return;
    if (id == 0) {
        if (value > 0) {
            if (selIdx_ < (mainMenu_.size() - 1)) selIdx_++;
        } else if (value < 0) {
            if (selIdx_ > 0) selIdx_--;
        }
        if (selIdx_ >= (menuOffset_ + maxItems_)) menuOffset_ = selIdx_ - (maxItems_ - 1);
        if (menuOffset_ > selIdx_) menuOffset_ = selIdx_;

        displayMenu();
    }

    osc::OutboundPacketStream ops(osc_write_buffer_, OSC_OUTPUT_BUFFER_SIZE);
    ops << osc::BeginBundleImmediate
        << osc::BeginMessage("/nui/encoder")
        << (int32_t) id
        << (int32_t) value
        << osc::EndMessage
        << osc::EndBundle;
    sendOsc(ops.Data(), ops.Size());
}

std::string SKApp::getCmdOptions(const std::string &file) {
    std::string opts;
    std::ifstream infile(file.c_str());
    std::string line;
    while (std::getline(infile, line)) {
        if (line.length() > 0) {
            opts += " " + line;
        }
    }
    return opts;
}


void *osc_proc(void *aThis) {
    SKApp *pThis = static_cast<SKApp *>(aThis);
    pThis->processOsc();
    return 0;
}

void SKApp::processOsc() {
    oscListenSocket_.reset(
        new UdpListeningReceiveSocket(
            IpEndpointName(IpEndpointName::ANY_ADDRESS, listenPort_),
            oscListener_.get())
    );
    oscListenSocket_->Run();
}

void *osc_write_proc(void *aThis) {
    SKApp *pThis = static_cast<SKApp *>(aThis);
    pThis->processOscWrite();
    return 0;
}


static constexpr unsigned OSC_WRITE_POLL_WAIT_TIMEOUT = 1000;

void SKApp::processOscWrite() {
    oscWriteSocket_ = std::shared_ptr<UdpTransmitSocket>(new UdpTransmitSocket(IpEndpointName("127.0.0.1", sendPort_)));
    while (keepRunning_) {
        OscMsg msg;
        if (writeMessageQueue_.wait_dequeue_timed(msg, std::chrono::milliseconds(OSC_WRITE_POLL_WAIT_TIMEOUT))) {
            oscWriteSocket_->Send(msg.buffer_, (size_t) msg.size_);
        }
    }
}


void SKApp::startOscServer() {
    oscListener_ = std::make_shared<SKOscPacketListener>(readMessageQueue_);
    oscProcessor_ = std::make_shared<SKOscCallback>(*this);
    osc_server_ = std::thread(osc_proc, this);
    osc_writer_ = std::thread(osc_write_proc, this);
}

void SKApp::sendOscEvent(const std::string &event) {
    osc::OutboundPacketStream ops(osc_write_buffer_, OSC_OUTPUT_BUFFER_SIZE);
    std::string addr = std::string("/nui/") + event;
    ops << osc::BeginBundleImmediate
        << osc::BeginMessage(addr.c_str())
        << osc::EndMessage
        << osc::EndBundle;

}

void SKApp::sendOsc(const char *data, unsigned size) {
    OscMsg msg;
    msg.size_ = (size > OscMsg::MAX_OSC_MESSAGE_SIZE ? OscMsg::MAX_OSC_MESSAGE_SIZE : size);
    memcpy(msg.buffer_, data, (size_t) msg.size_);
    writeMessageQueue_.enqueue(msg);
}


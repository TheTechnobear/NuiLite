#include "SKApp.h"
#include "SKHardwareCallback.h"
#include "SKOscCallback.h"
#include "SKPrefs.h"

#include "NuiDevice.h"
#include <unistd.h>

#include <dirent.h>
#include <sys/stat.h>

#include <iostream>
#include <clocale>

// osc
#include <osc/OscOutboundPacketStream.h>
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
    scOpts_ = prefs.getString("scOpts", "");
    scHome_ = prefs.getString("scHome", ".");

    listenPort_ = (unsigned) prefs.getInt("listenPort", 3001);
    sendPort_ = (unsigned) prefs.getInt("sendPort", 3000);
    sendAddr_ = prefs.getString("sendAddr", "127.0.0.1");

    topPatchDir_ = patchDir_;
    topSystemDir_ = systemDir_;

    bool menuOnStartup = prefs.getBool("menuOnStartup", false);
    std::setlocale(LC_ALL, "en_US.UTF-8");

    cb->init();
    device_.addCallback(cb);
    device_.start();

    startOscServer();
    sendSKOscEvent("start");

    if (checkFileExists("./sidekick_starting.sh")) {
        execShell("./sidekick_starting.sh");
    }

    SKPrefs stat(stateFile_);
    std::string lastPatch, lastPatchDir = patchDir_;
    bool lastSys;
    if (stat.valid()) {
        lastSys = stat.getBool("lastPatchSys", false);
        lastPatch = stat.getString("lastPatch", "");
        if (lastSys) {
            systemDir_ = stat.getString("lastPatchDir", patchDir_);
        } else {
            patchDir_ = stat.getString("lastPatchDir", systemDir_);
        }
    }

    reloadMenu();

    bool lastPatchFound = false;
    unsigned idx = 0;
    for (const auto &mi:mainMenu_) {
        if (mi->name_ == lastPatch) {
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
                std::cerr << "load startup patch: " << lastPatch << std::endl;
                activateItem();
            }
        }
    }
}

void SKApp::stop() {
    stopPatch();
    sendSKOscEvent("stop");
    //std::cerr << "SKApp::stop" << std::endl;
    keepRunning_ = false;
    oscListenSocket_->AsynchronousBreak();

    if (osc_server_.joinable()) {
        osc_server_.join();
    }
    oscListenSocket_.reset();

    if (osc_writer_.joinable()) {
        osc_writer_.join();
    }
    oscWriteSocket_.reset();
    device_.stop();
    //std::cerr << "SKApp::run end" << std::endl;
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
    sendSKOscEvent("stopPatch");
    sleep(1);

    if (lastActiveDir_.length() > 0) {
        std::string stopfile = lastActiveDir_ + "/stop.sh";
        if (checkFileExists(stopfile)) {
            std::string shcmd = "cd \"" + lastActiveDir_ + "\"; ./stop.sh";
            execShell(shcmd);
        }
    }

    switch (lastActiveType_) {
        case MenuItem::PdPatch : {
            execShell("killall pd &");
            if (checkFileExists("./stop_pd.sh")) {
                execShell("./stop_pd.sh");
            }
	    break;
        }
        case MenuItem::ScPatch : {
            execShell("killall sclang scsynth &");
            if (checkFileExists("./stop_sc.sh")) {
                execShell("./stop_sc.sh");
            }
	    break;
        }
        default: {
            break;
        }
    }

    if (checkFileExists("./stop_all.sh")) {
        execShell("./stop_all.sh");
    }

    displayMenu();
    device_.displayPaint();
    sidekickActive_ = true;
}

int SKApp::execShell(const std::string &cmd) {
    std::cout << "exec : " << cmd << std::endl;
    return system(cmd.c_str());
}


void SKApp::runScript(const std::string &root, const std::shared_ptr<MenuItem> &item, const std::string &cmd) {
    const std::string &name = item->name_;
    std::string shcmd = "cd \"" + root + "/" + name + "\"; ./" + cmd + " &";
    execShell(shcmd);
}

void SKApp::runDir(const std::string &, const std::shared_ptr<MenuItem> &item) {
    const std::string &name = item->name_;
    std::string dir = patchDir_;
    if (item->system_) {
        dir = systemDir_;
    }

    if (name == std::string("..")) {
        int idx = dir.find_last_of('/');
        if (idx != std::string::npos) {
            dir = dir.substr(0, idx);
        }
    } else {
        dir = dir + "/" + name;
    }

    if(item->system_) {
	    systemDir_=dir;
    } else {
	    patchDir_=dir;
    }
    runRefreshMenu();
}

std::string SKApp::getFile(const std::string& topDir,const std::string& dir, const std::string& file) {
    std::string foundfile;
    if (checkFileExists(dir + "/" + file)) return  dir + "/"  + file;
    if (checkFileExists(topDir + "/" + file)) return topDir + "/" +file;
    if (checkFileExists("./" + file)) return  "./" + file;
    return "";
}

void SKApp::runPd(const std::string &, const std::shared_ptr<MenuItem> &item) {
    const std::string &name = item->name_;
    std::string pdOpts = pdOpts_;
    int nogui = execShell("xprop -root > /dev/null");
    if (!nogui) {
        // overides existing options
        pdOpts = pdOpts + " -gui -audiobuf 10";
    }

    std::string dir = patchDir_;
    std::string topDir = topPatchDir_;
    if (item->system_) {
        dir = systemDir_;
        topDir = topSystemDir_;
    }

    std::string optsFile= getFile(dir,dir+"/"+name, "pd-opts.txt");
    if (optsFile.length() > 0) {
        pdOpts = pdOpts + " " + getCmdOptions(optsFile);
        std::cout << "using cmd options file : " << optsFile << " options : " << pdOpts << std::endl;
    }
    std::string startFile= getFile(dir,dir+"/"+name, "start_pd.sh");
    if (startFile.length() > 0) {
        std::cout << "using start file : "<< startFile << std::endl;
        execShell("\"" + startFile + "\"");
    }

    std::string shcmd = "cd \"" + dir + "/" + name + "\"; pd " + pdOpts + " main.pd &";
    std::cout << "pd running : " << shcmd << std::endl;
    execShell(shcmd);
}


void SKApp::runSc(const std::string &, const std::shared_ptr<MenuItem> &item) {
    const std::string &name = item->name_;
    std::string scOpts = scOpts_;
//    int nogui = execShell("xprop -root > /dev/null");

    std::string dir = patchDir_;
    std::string topDir = topPatchDir_;
    if (item->system_) {
        dir = systemDir_;
        topDir = topSystemDir_;
    }

    std::string optsFile= getFile(dir,dir+"/"+name, "sc-opts.txt");
    if (optsFile.length() > 0) {
        scOpts = scOpts + " " + getCmdOptions(optsFile);
        std::cout << "using cmd options file : " << optsFile << " options : " << scOpts << std::endl;
    }

    std::string startFile= getFile(dir,dir+"/"+name, "start_sc.sh");
    if (startFile.length() > 0) {
        std::cout << "using start file : "<< startFile << std::endl;
        execShell("\"" + startFile + "\"");
    }
    std::string shcmd = "cd \"" + dir + "/" + name + "\"; HOME=" + scHome_ + " sclang " + scOpts + " main.scd &  ";
    std::cout << "sc running : " << shcmd << std::endl;
    execShell(shcmd);
}


void SKApp::runInstall(const std::string &root, const std::shared_ptr<MenuItem> &item) {
    const std::string &name = item->name_;
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
    sendSKOscEvent("update");
    std::cout << "refresh system" << std::endl;
    sidekickActive_ = true;
    // just in case, something has been manually added!
    reloadMenu();

    device_.displayClear();
    device_.displayText(15, 0, 0, "Checking for updates ...");
    device_.displayText(15, 2, 0, "Updating repo...");
    device_.displayPaint();

    execShell("sudo apt update");

    // recursively look thru diretories for update.sh to update patches etc
    runUpdate(topSystemDir_);
    runUpdate(topPatchDir_);

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

void SKApp::runUpdate(const std::string &root) {
    struct dirent **namelist;
    int n = scandir(root.c_str(), &namelist, nullptr, alphasort);
    if (n <= 0) return;

    for (int i = 0; i < n; i++) {
        char *fname = namelist[i]->d_name;
        if (fname[0] != '.') {
            switch (namelist[i]->d_type) {
                case DT_DIR : {
                    std::string dir = root + "/" + fname;
                    std::string updatefile = dir + "/update.sh";
                    if (checkFileExists(updatefile)) {
                        std::string txt = "Checking " + dir + " ...";
                        device_.clearText(0, 2);
                        device_.displayText(15, 2, 0, txt);
                        device_.displayPaint();
                        std::string shcmd = "cd \"" + dir + "\"; ./update.sh";
                        std::cout << "update running : " << shcmd << std::endl;
                        execShell(shcmd);
                    } else {
                        runUpdate(dir);
                    }
                    break;
                }
                default:
                    break;
            }// switch
        }
        free(namelist[i]);
    }
    free(namelist);
}


void SKApp::runPowerOff() {
    sendSKOscEvent("powerOff");
    sleep(1);
    device_.displayClear();
    device_.displayText(15, 0, 0, "Powering Down...");
    device_.displayPaint();
    execShell("sudo poweroff");
}


int SKApp::checkFileExists(const std::string &filename) {
    //std::cerr << "checkFileExists : [" << filename << "]" << std::endl;
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
        else if ((*mi)->type_ == MenuItem::Dir) txt = "[" + txt + "]";
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

        device_.displayPaint();
        sidekickActive_ = false;
        std::string dir = patchDir_;
        std::string topDir = topPatchDir_;
        if (item->system_) {
            dir = systemDir_;
            topDir = topSystemDir_;
        }

        sendSKOscEvent("activateItem", dir + "/" + item->name_);

        lastActiveDir_ = dir + "/" + item->name_;
        lastActiveType_ = item->type_;

        switch (item->type_) {
            case MenuItem::System: {
                runScript(dir, item, "run.sh");
                break;
            }
            case MenuItem::PdPatch:
            case MenuItem::ScPatch:
            case MenuItem::ShellPatch: {

                if (loadOnStartup_ && stateFile_.length()) {
                    saveState(item);
                }

                std::string prePatch= getFile(dir,dir+"/"+item->name_, "pre-patch.sh");
                if (prePatch.length() > 0) execShell("\"" + prePatch + "\"");

                if (item->type_ == MenuItem::PdPatch) {
                    runPd(dir, item);
                } else if (item->type_ == MenuItem::ScPatch) {
                    runSc(dir, item);
                } else {
                    runScript(dir, item, "run.sh");
                }

                std::string postPatch= getFile(dir,dir+"/"+item->name_, "post-patch.sh");
                if (postPatch.length() > 0) execShell("\"" + postPatch + "\"");

                break;
            }
            case MenuItem::InstallFile : {
                runInstall(dir, item);
                break;
            }
            case MenuItem::Dir : {
                runDir(dir, item);
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
            case MenuItem::MAX_ITEMS:
            default:
                break;
        }
    } catch (std::out_of_range &) {

    }
}

void SKApp::saveState(const std::shared_ptr<MenuItem> &item) {
    if (stateFile_.length() == 0) return;

    std::ofstream outfile(stateFile_.c_str());
    cJSON *root = cJSON_CreateObject();

    if (item) {
        std::string dir = patchDir_;
        if (item->system_) {
            dir = systemDir_;
            cJSON_AddTrueToObject(root, "lastPatchSys");
        } else {
            cJSON_AddFalseToObject(root, "lastPatchSys");
        }
        cJSON_AddStringToObject(root, "lastPatch", item->name_.c_str());
        cJSON_AddStringToObject(root, "lastPatchDir", dir.c_str());
    }

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

    if(systemDir_==topSystemDir_) {
        loadMenu(patchDir_, false);
    }

    if(patchDir_==topPatchDir_) {
        loadMenu(systemDir_, true);
    }

    if(patchDir_==topPatchDir_ && systemDir_ == topSystemDir_) {
        mainMenu_.push_back(std::make_shared<MenuItem>("Check for Updates", MenuItem::RefreshSystem, true));
        mainMenu_.push_back(std::make_shared<MenuItem>("Refresh Menu", MenuItem::RefreshMenu, true));
    }
    mainMenu_.push_back(std::make_shared<MenuItem>("Power OFF ", MenuItem::PowerOff, true));
}


void SKApp::loadMenu(const std::string &dir, bool sys) {
    std::string topDir = topPatchDir_;
    if (sys) {
        topDir = topSystemDir_;
    }

    if(topDir!=dir) {
	auto menuItem = std::make_shared<MenuItem>("..", MenuItem::Dir, sys);
	mainMenu_.push_back(menuItem);
    }

    struct dirent **namelist;
    int n = scandir(dir.c_str(), &namelist, nullptr, alphasort);
    if (n <= 0) return;

    for (int i = 0; i < n; i++) {
        char *fname = namelist[i]->d_name;
        if (fname[0] != '.') {
            switch (namelist[i]->d_type) {
                case DT_DIR : {
                    std::string patchlocation = dir + "/" + fname;
                    std::string scfile = patchlocation + "/main.scd";
                    std::string mainpd = patchlocation + "/main.pd";
                    std::string shellfile = patchlocation + "/run.sh";
                    bool pd = checkFileExists(mainpd);
                    bool sc = checkFileExists(scfile);
                    bool sh = checkFileExists(shellfile);
                    if (pd) {
                        auto menuItem = std::make_shared<MenuItem>(fname, MenuItem::PdPatch, sys);
                        mainMenu_.push_back(menuItem);
                    } else if (sc) {
                        auto menuItem = std::make_shared<MenuItem>(fname, MenuItem::ScPatch, sys);
                        mainMenu_.push_back(menuItem);
                    } else if (sh) {
                        auto menuItem = std::make_shared<MenuItem>(fname, sys ? MenuItem::System : MenuItem::ShellPatch, sys);
                        mainMenu_.push_back(menuItem);
                    } else {
                        bool ignore = checkFileExists(patchlocation + "/ignore.txt");
                        if (!ignore) {
                            auto menuItem = std::make_shared<MenuItem>(fname, MenuItem::Dir, sys);
                            mainMenu_.push_back(menuItem);
                        }
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
                            auto menuItem = std::make_shared<MenuItem>(fname, MenuItem::InstallFile, sys);
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
    osc::OutboundPacketStream ops(osc_write_buffer_, OSC_OUTPUT_BUFFER_SIZE);
    ops << osc::BeginBundleImmediate
        << osc::BeginMessage("/nui/encoder")
        << (int32_t) id
        << (int32_t) value
        << osc::EndMessage
        << osc::EndBundle;
    sendOsc(ops.Data(), ops.Size());

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
    auto *pThis = static_cast<SKApp *>(aThis);
    pThis->processOsc();
    return nullptr;
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
    auto *pThis = static_cast<SKApp *>(aThis);
    pThis->processOscWrite();
    return nullptr;
}


static constexpr unsigned OSC_WRITE_POLL_WAIT_TIMEOUT = 1000;

void SKApp::processOscWrite() {
    oscWriteSocket_ = std::shared_ptr<UdpTransmitSocket>(new UdpTransmitSocket(IpEndpointName(sendAddr_.c_str(), sendPort_)));
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

void SKApp::sendSKOscEvent(const std::string &event, const std::string data) {
    osc::OutboundPacketStream ops(osc_write_buffer_, OSC_OUTPUT_BUFFER_SIZE);
    std::string addr = std::string("/sk/") + event;
    ops << osc::BeginBundleImmediate
        << osc::BeginMessage(addr.c_str());

    if (data.length()) {
        ops << data.c_str();
    }

    ops << osc::EndMessage
        << osc::EndBundle;
    sendOsc(ops.Data(), ops.Size());
}

void SKApp::sendDeviceInfo() {
    osc::OutboundPacketStream ops(osc_write_buffer_, OSC_OUTPUT_BUFFER_SIZE);
    ops << osc::BeginBundleImmediate;
    
    ops << osc::BeginMessage("/nui/numbuttons")
        << 3
        << osc::EndMessage;

    ops << osc::BeginMessage("/nui/numencoders")
        << (int) device_.numEncoders()
        << osc::EndMessage;

    ops << osc::EndBundle;
    sendOsc(ops.Data(), ops.Size());
}

void SKApp::sendOsc(const char *data, unsigned size) {
    OscMsg msg;
    msg.size_ = (size > OscMsg::MAX_OSC_MESSAGE_SIZE ? OscMsg::MAX_OSC_MESSAGE_SIZE : size);
    memcpy(msg.buffer_, data, (size_t) msg.size_);
    writeMessageQueue_.enqueue(msg);
}


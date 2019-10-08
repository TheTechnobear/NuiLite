#include "SKApp.h"
#include "SKCallback.h"
#include "SKPrefs.h"

#include "NuiDevice.h"
#include <unistd.h>

#include <dirent.h>
#include <sys/stat.h>

#include <iostream>
#include <iomanip>
#include <clocale>

// for saving state
#include <fstream>
#include <cJSON.h>

SKApp::SKApp() :
    sidekickActive_(false),
    selIdx_(0),
    keepRunning_(true) {

}


void SKApp::init(SKPrefs &prefs) {
    auto cb = std::make_shared<SKCallback>(*this);
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

    std::setlocale(LC_ALL, "en_US.UTF-8");
    mainMenu_.clear();
    loadMenu(patchDir_, false);
    loadMenu(systemDir_, true);


    cb->init();

    device_.addCallback(cb);
    device_.start();

    if (loadOnStartup_) {

        if (device_.buttonState(0)) {
            std::cerr << "button 1 during startup : force menu" << std::endl;
            activeCount_ = 1;
        } else {
            SKPrefs stat(stateFile_);
            if (!stat.valid()) {
                std::cerr << "state file invalid , bring up menu" << std::endl;
                activeCount_ = 1;
            } else {
                lastPatch_ = stat.getString("lastPatch", "");
                if (lastPatch_.length() == 0) {
                    std::cerr << "no last patch, bring up menu" << std::endl;
                    activeCount_ = 1;
                } else {
                    std::cerr << "load startup file" << std::endl;
                    bool loaded = false;
                    std::string patchlocation = patchDir_ + "/" + lastPatch_;
                    std::string shellfile = patchlocation + "/run.sh";
                    std::string mainpd = patchlocation + "/main.pd";
                    if (checkFileExists(shellfile)) {
                        runScript(patchDir_, lastPatch_, "run.sh");
                        loaded = true;
                    } else if (checkFileExists(mainpd)) {
                        runPd(patchDir_, lastPatch_);
                        loaded = true;
                    };

                    if (loaded) {
                        unsigned idx = 0;
                        for (const auto &mi:mainMenu_) {
                            if (mi->name_ == lastPatch_) {
                                selIdx_ = idx;
                                break;
                            }
                            idx++;
                        }
                    } else {
                        // bring up menu if we could not load patch
                        activeCount_ = 1;
                    }
                }
            }
        }
    }
}

void SKApp::stop() {
    //std::cerr << "SKApp::stop" << std::endl;
    keepRunning_ = false;
}


void SKApp::run() {
    while (keepRunning_) {
        device_.process(sidekickActive_);
        if (activeCount_ > 0) {
            activeCount_--;
            if (activeCount_ <= 0) {
                activeCount_ = -1;
                sidekickActive_ = true;
                if (sidekickActive_) {
                    std::cerr << "Sidekick activated,stop processes" << std::endl;
                    displayMenu();
                    for (const auto &mi:mainMenu_) {
                        std::string root;
                        if (mi->type_ == MenuItem::System) {
                            root = systemDir_;
                        } else {
                            root = patchDir_;
                        }
                        std::string stopfile = root + "/" + mi->name_ + "/stop.sh";
                        if (checkFileExists(stopfile)) {
                            runScript(root, mi->name_, "stop.sh");
                        } else if (mi->type_ == MenuItem::PdPatch) {
                            execShell("killall pd &");
                        }
                    }
                }
            }
        }
        usleep(POLL_MS_ * 1000);
    }
    //std::cerr << "SKApp::run end" << std::endl;
    device_.stop();
}


int SKApp::execShell(const std::string &cmd) {
    return system(cmd.c_str());
}


void SKApp::runScript(const std::string &root, const std::string &name, const std::string &cmd) {
    std::string shcmd = root + "/" + name + "/" + cmd + " &";
    std::cout << "script running : " << shcmd << std::endl;
    execShell(shcmd);
}


void SKApp::runPd(const std::string &root, const std::string &name) {
    std::string shcmd = "cd " + root + "/" + name + "; pd " + pdOpts_ + " main.pd &";
    std::cout << "pd running : " << shcmd << std::endl;
    execShell(shcmd);
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
        device_.displayText(15, line, 0, (*mi)->name_);
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
        sleep(1);
        sidekickActive_ = false;
        std::string root;
        if (item->type_ == MenuItem::System) {
            root = systemDir_;
        } else {
            root = patchDir_;
        }

        std::string runFile = root + "/" + item->name_ + "/run.sh";
        if (checkFileExists(runFile)) {
            if (item->type_ == MenuItem::ShellPatch) {
                lastPatch_ = item->name_;
                if (loadOnStartup_ && stateFile_.length()) {
                    saveState();
                }
            }

            runScript(root, item->name_, "run.sh");
        }

    } catch (std::out_of_range &) { ;
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


void SKApp::loadMenu(const std::string dir, bool sys) {
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
                    if ( pd ) {
                        auto menuItem = std::make_shared<MenuItem>(fname, MenuItem::PdPatch);
                        mainMenu_.push_back(menuItem);
                    } else if(sh){
                        auto menuItem = std::make_shared<MenuItem>(fname, sys ? MenuItem::System : MenuItem::ShellPatch);
                        mainMenu_.push_back(menuItem);
                    }
                    break;
                } //DT_DIR
                case DT_REG: {
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
            activeCount_ = ACTIVE_TIME_;
        } else {
            activeCount_ = -1;
        }
    } else if (id == 0 && value) {
        std::cerr << "Sidekick launch " << std::endl;
        activateItem();
    }
}

void SKApp::onEncoder(unsigned id, int value) {
    if (!sidekickActive_) return;
    if (id == 0) {
        if (value > 0) {
            if (selIdx_ < maxItems_ && selIdx_ < (mainMenu_.size() - 1)) selIdx_++;
        } else if (value < 0) {
            if (selIdx_ > 0) selIdx_--;
        }
        if (selIdx_ > (menuOffset_ + maxItems_)) menuOffset_ = selIdx_ - maxItems_;
        if (menuOffset_ > selIdx_) menuOffset_ = selIdx_;
    }
    displayMenu();
}

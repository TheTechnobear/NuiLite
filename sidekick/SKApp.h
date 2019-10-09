#pragma once


#include <string>
#include <memory>
#include <vector>

#include "NuiDevice.h"

class SKPrefs;

class SKApp {
public:
    SKApp();

    void init(SKPrefs &);
    void run();
    void stop();

    void onButton(unsigned id, unsigned value);
    void onEncoder(unsigned id, int value);

private:
    struct MenuItem {
        enum Type {
            ShellPatch,
            PdPatch,
            ZipFile,
            System,
            RefreshMenu
        } type_;

        explicit MenuItem(const std::string &n, MenuItem::Type t) : name_(n), type_(t) { ; }

        std::string name_;
    };

    static int execShell(const std::string &cmd);
    static void runScript(const std::string &root,const std::string &name, const std::string &cmd);
    void runPd(const std::string &root, const std::string &name);
    void runZip(const std::string &root, const std::string &name);
    void runRefreshMenu();

    static int checkFileExists(const std::string &filename);
    void displayMenu();
    void activateItem();
    void saveState();
    void reloadMenu();
    void loadMenu(const std::string& dir, bool sys);
    static std::string  getCmdOptions(const std::string& file);

    unsigned POLL_MS_ = 1;
    unsigned ACTIVE_TIME_ = 5000; //5000 mSec
    std::string patchDir_;
    std::string systemDir_;
    int activeCount_ = -1;

    NuiLite::NuiDevice device_;


    std::vector<std::shared_ptr<MenuItem>> mainMenu_;

    bool sidekickActive_ = false;
    unsigned selIdx_ = 0;
    unsigned menuOffset_ = 0;
    bool buttonState_[4] = {false, false, false, false};
    unsigned maxItems_ = 6;
    bool keepRunning_;

    std::string lastPatch_;

    bool loadOnStartup_;
    std::string stateFile_;
    std::string pdOpts_;
};

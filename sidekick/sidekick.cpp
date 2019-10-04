#include <iostream>
#include <memory>

#include <unistd.h>
#include <signal.h>

#include "SKApp.h"
#include "SKPrefs.h"

static volatile bool keepRunning = true;

class SKApp;

std::shared_ptr<SKApp> app_ = nullptr;


void intHandler(int) {
    std::cerr << "Sidekick intHandler called" << std::endl;
    if (!keepRunning) {
        sleep(2);
        exit(-1);
    }
    if (app_ != nullptr) app_->stop();
}


int main(int argc, const char *argv[]) {
    SKPrefs prefs;
    if (!prefs.loadPreferences(std::string("./sidekick.json"))) {
        std::cerr << "./sidekick.json preferences file cannot be loaded";
        return -1;
    }

    std::cout << "starting sidekick" << std::endl;
    signal(SIGINT, intHandler);
    app_ = std::make_shared<SKApp>();
    app_->init(prefs);
    app_->run();
    std::cout << "stopping sidekick" << std::endl;
    return 0;
}

#include "NuiDevice.h"
#include <unistd.h>
#include <signal.h>

#include <dirent.h>
#include <sys/stat.h>

#include <vector>
#include <iostream>
#include <iomanip>


static volatile bool keepRunning = 1;

static unsigned POLL_MS=1;
static int activeCount=-1;
static unsigned ACTIVE_TIME= 5000; //5000 mSec

NuiLite::NuiDevice device;

void intHandler(int dummy) {
    std::cerr << "Sidekick intHandler called" << std::endl;
    if(!keepRunning) {
        sleep(1);
        exit(-1);
    }
    keepRunning = false;
    device.stop();
}

static bool sidekickActive_=false;
static int selIdx_=0;

struct MenuItem {
    MenuItem(const std::string& n) : name_(n) { ; }
    std::string name_;
};


std::vector<std::shared_ptr<MenuItem>> mainMenu;

std::string patchDir="./patches";

int  execShell(const std::string& cmd) {
    return system(cmd.c_str());
}


void runScript(const std::string& name,const std::string&  cmd) {
    std::string shcmd = patchDir + "/" + name +"/" + cmd + " &";
    std::cout << "running : " << shcmd << std::endl;
    execShell(shcmd);
}


int checkFileExists (const std::string& filename) {
    struct stat st;
    int result = stat(filename.c_str(), &st);
    return result == 0;
}

void displayMenu() {
    device.displayClear();
    int line=0;
    for(auto mi : mainMenu) {
        device.displayText(line,mi->name_);
        line++;
        if(line>5) break;
    }
    device.invertText(selIdx_);
}


void activateItem() {
    try {
        auto item=mainMenu.at(selIdx_);
        device.displayClear();
        device.displayText(0,"Launch...");
        device.displayText(1,item->name_);
        std::cerr << "launch : " << item->name_ << std::endl;
        sleep(1);
        sidekickActive_=false;
        std::string runFile=patchDir+"/"+item->name_+"/run.sh";
        if(checkFileExists(runFile)) {
            runScript(item->name_,"run.sh");
        }

    } catch(std::out_of_range) {
    }
}

class SKCallback : public NuiLite::NuiCallback {
public:
    void init() {
        selIdx_=0;
        sidekickActive_=false;


        struct dirent **namelist;
        std::setlocale(LC_ALL, "en_US.UTF-8");

        int n = scandir(patchDir.c_str(), &namelist, NULL, alphasort);

        for (int i = 0; i < n; i++) {
            char* fname = namelist[i]->d_name;
            if (fname[0]!='.') {
                switch(namelist[i]->d_type) {
                case DT_DIR : {
                    std::string patchlocation = patchDir + "/" + fname;
                    std::string mainpd = patchlocation + "/main.pd";
                    std::string shellfile = patchlocation + "/run.sh";
                    if (     checkFileExists(mainpd)
                             ||  checkFileExists(shellfile)
                       ) {
                        auto menuItem=std::make_shared<MenuItem>(fname);
                        mainMenu.push_back(menuItem);
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

    void onButton(unsigned id, unsigned value)  override {
        buttonState_[id]=value;
        if(!sidekickActive_) {
            bool all3=true;
            for(int i=0;i<3;i++) {
                all3 = all3 & buttonState_[i];
            }

            if(all3) {
                activeCount=ACTIVE_TIME;
            } else {
                activeCount=-1;
            }
        } else if(id==0 && value) {
                std::cerr << "Sidekick launch " << std::endl;
                activateItem();
        }
    }

    void onEncoder(unsigned id, int value) override  {
        if(!sidekickActive_) return;
        if(id==0) {
            if(value>0) {
                selIdx_++;
                selIdx_ = selIdx_>maxItems_ ? maxItems_ : selIdx_;
                selIdx_ = selIdx_>mainMenu.size()-1 ? mainMenu.size()-1 : selIdx_;
            } else if (value < 0) {
                selIdx_--;
                selIdx_ = selIdx_<0 ? 0 : selIdx_;
            }
        }
        displayMenu();
    }


private:
    unsigned buttonState_[4] = {false,false,false,false};
    unsigned maxItems_=5;
};


int main(int argc, const char * argv[]) {
    std::cout << "starting sidekick" << std::endl;
    auto cb = std::make_shared<SKCallback>();
    cb->init();
    device.addCallback(cb);

    device.start();

    signal(SIGINT, intHandler);

    while(keepRunning) {
        device.process(sidekickActive_);
        if(activeCount>0) {
            activeCount--;
            if(activeCount<=0) {
                activeCount=-1;
                sidekickActive_=true;
                if(sidekickActive_) {
                    std::cerr << "Sidekick activated,stop processes" << std::endl;
                    displayMenu();
                    for(auto mi:mainMenu) {
                        std::string stopfile = patchDir+"/" + mi->name_+"/stop.sh";
                        if(checkFileExists(stopfile)) {
                            runScript(mi->name_,"stop.sh");
                        }
                    }
                }
            }
        }
        usleep(POLL_MS*1000);
    }
    std::cout << "stopping sidekick" << std::endl;
    device.stop();
    return 0;
}

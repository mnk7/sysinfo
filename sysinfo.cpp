#include <iostream>
#include <fstream>
#include <vector>
#include <getopt.h>
#include <chrono>
#include <thread>

// get terminal size
#include <sys/ioctl.h>
#include <unistd.h>


void changeColor(uint color) {
    switch(color) {
    // blue
    case 2:
        printf("\x1b[34m");
        break;
    // yellow
    case 3:
        printf("\x1b[33m");
        break;
    // red
    case 4:
        printf("\x1b[31m");
        break;
    // green
    case 5:
        printf("\x1b[32m");
        break;

    // thick white
    case 1:
        printf("\x1b[0;1m");
        break;
    // white
    case 0:
    default:
        printf("\x1b[0m");
        break;
    }
}


/**
 * Sets Cursor to specified coordinates
 * @brief gotoxy
 * @param x
 * @param y
 */
void gotoxy(int x,int y) {
    printf("%c[%d;%df", 0x1B, y, x);
}


/**
 * Clears the Screen
 * @brief clear
 */
void clear() {
    printf("\e[1;1H\e[2J");
}


double getCPUMaxBaseclock() {
    std::ifstream cpuinfo;
    cpuinfo.open("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq", std::ifstream::in);

    std::string in;
    std::getline(cpuinfo, in);
    in = in.substr(0, in.find_first_of(" "));

    cpuinfo.close();

    return std::stod(in);
}


std::vector<double> readCPUFrequency() {
    std::vector<double> frequencies;

    int core = 0;
    std::string in;
    std::ifstream cpuinfo;
    cpuinfo.open("/sys/devices/system/cpu/cpu" + std::to_string(core) + "/cpufreq/scaling_cur_freq", std::ifstream::in);

    while(!cpuinfo.fail()) {
        std::getline(cpuinfo, in);
        frequencies.push_back(std::stod(in));

        ++core;
        cpuinfo.close();
        cpuinfo.open("/sys/devices/system/cpu/cpu" + std::to_string(core) + "/cpufreq/scaling_cur_freq", std::ifstream::in);
    }

    cpuinfo.close();

    return frequencies;
}


std::vector<double> readMemoryInfo() {
    // MemTotal - MemFree - MemAvailable - Cached - SwapTotal - SwapFree - SwapCached
    std::vector<double> info(7);

    std::ifstream meminfo;
    meminfo.open("/proc/meminfo", std::ifstream::in);

    std::string in;
    while(std::getline(meminfo, in)) {
        std::string parameter = in.substr(0, in.find_first_of(":"));
        std::string value = in.substr(parameter.size());
        value = value.substr(value.find_first_not_of(": \t"));
        value = value.substr(0, value.find_first_of(" "));

        if(parameter == "MemTotal") {
            info[0] = std::stod(value);
            continue;
        }
        if(parameter == "MemFree") {
            info[1] = std::stod(value);
            continue;
        }
        if(parameter == "MemAvailable") {
            info[2] = std::stod(value);
            continue;
        }
        if(parameter == "Cached") {
            info[3] = std::stod(value);
            continue;
        }
        if(parameter == "SwapTotal") {
            info[4] = std::stod(value);
            continue;
        }
        if(parameter == "SwapFree") {
            info[5] = std::stod(value);
            continue;
        }
        if(parameter == "SwapCached") {
            info[6] = std::stod(value);
            continue;
        }
    }

    return info;
}


void updateGraph(double baseclock) {
    // get terminal size
    struct winsize size;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);

    auto data = readCPUFrequency();
    clear();
    gotoxy(0, 0);
    changeColor(0);

    // still available columns after label: (22 columns)
    uint available_columns = 5;
    if(size.ws_col > 25) {
        available_columns = size.ws_col - 21;
    }
    double scaling_factor = available_columns / (3 * baseclock);

    for(decltype(data.size()) core = 0; core < data.size(); ++core) {
        gotoxy(0, core + 2);
        printf("  CPU %lu:   %d MHz ", core, static_cast<uint>(data[core]));
        changeColor(1);
        printf("[");

        changeColor(5);
        // The whole area should be 3x baseclock
        for(uint bar = 0; bar < static_cast<uint>(data[core] * scaling_factor); ++bar) {
            if(bar >= baseclock * scaling_factor) {
                changeColor(2);
            }
            printf("|");
        }

        printf("\n");
        changeColor(0);
    }


    uint current_line = data.size() + 2;
    // we now need a column for a ']' at the end
    available_columns -= 1;

    data = readMemoryInfo();
    if(data[0] > data[4]) {
        scaling_factor = available_columns / (1.5 * data[0]);
    } else {
        scaling_factor = available_columns / (1.5 * data[4]);
    }

    // print RAM info
    gotoxy(0, current_line + 1);
    uint used_memory = static_cast<uint>((data[0] - data[2]) / 1e3);
    // take care of proper alignment:
    if(used_memory < 1e2) {
        printf("  MEMORY:     %d MB ", used_memory);
    } else if(used_memory < 1e3) {
        printf("  MEMORY:    %d MB ", used_memory);
    } else if(used_memory < 1e4) {
        printf("  MEMORY:   %d MB ", used_memory);
    } else if(used_memory < 1e5) {
        printf("  MEMORY:  %d MB ", used_memory);
    } else {
        printf("  MEMORY: %d MB ", used_memory);
    }
    changeColor(1);
    printf("[");

    changeColor(4);
    // used = total - available
    // The whole area should be equal to the total memory
    for(uint bar = 0; bar < static_cast<uint>((data[0] - data[2]) * scaling_factor); ++bar) {
        printf("|");
    }

    changeColor(3);
    // cached memory
    for(uint bar = 0; bar < static_cast<uint>(data[3] * scaling_factor); ++bar) {
        printf("|");
    }

    changeColor(1);
    // fill the bracket to show the free amount of memory
    for(uint bar = 0; bar < static_cast<uint>((data[0] + data[0] - data[2] - data[3]) * scaling_factor); ++bar) {
        printf(" ");
    }
    printf("]\n");
    changeColor(0);


    if(data[4] > 0) {
        // print Swap info
        gotoxy(0, current_line + 2);
        used_memory = static_cast<uint>((data[4] - data[5]) / 1e3);
        // take care of proper alignment:
        if(used_memory < 1e2) {
            printf("  SWAP:       %d MB ", used_memory);
        } else if(used_memory < 1e3) {
            printf("  SWAP:      %d MB ", used_memory);
        } else if(used_memory < 1e4) {
            printf("  SWAP:     %d MB ", used_memory);
        } else if(used_memory < 1e5) {
            printf("  SWAP:    %d MB ", used_memory);
        } else {
            printf("  SWAP:   %d MB ", used_memory);
        }
        changeColor(1);
        printf("[");

        changeColor(4);
        // used = total - free
        // The whole area should be equal to the total memory
        for(uint bar = 0; bar < static_cast<uint>((data[4] - data[5]) * scaling_factor); ++bar) {
            printf("|");
        }

        changeColor(3);
        // cached memory
        for(uint bar = 0; bar < static_cast<uint>(data[6] * scaling_factor); ++bar) {
            printf("|");
        }

        changeColor(1);
        // fill the bracket to show the free amount of memory
        for(uint bar = 0; bar < static_cast<uint>((data[4] + data[4] - data[5] - data[6]) * scaling_factor); ++bar) {
            printf(" ");
        }
        printf("]\n");
        changeColor(0);
    }

}


int main(int argc, char*argv[]) {
    uint update_interval_ms = 1000;
    double baseclock = getCPUMaxBaseclock();

    int c;
    while ((c = getopt (argc, argv, "u:")) != -1) {
        switch(c) {
            case 'u':
                update_interval_ms = std::stoul(optarg);
                break;
            default:
                break;
        }
    }

    clear();

    while(true) {
        updateGraph(baseclock);
        std::this_thread::sleep_for(std::chrono::milliseconds(update_interval_ms));
    }
}

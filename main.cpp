#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>

// #include <nlohmann/json.hpp> // Required for parsing config.json

#include "Core/Globals.h"
#include "Core/Emu.h"
#include "Core/Utils.h"
#include <unicorn/unicorn.h>

void info() {
    uint32_t major = 0, minor = 0;
    std::cout << "\n";
    std::cout << "Cmulator Malware Analyzer - By Coldzer0\n\n";
    std::cout << "Compiled on      : " << __DATE__ << " - " << __TIME__ << "\n";
    std::cout << "Target CPU       : i386 & x86_x64\n";
    
    // uc_version(&major, &minor);
    // std::cout << "Unicorn Engine   : v" << major << "." << minor << " \n";
    std::cout << "Unicorn Engine   : (linked)\n";
    std::cout << "QJS VERSION      : (linked)\n";
    std::cout << "Cmulator         : " << CM_VERSION << "\n";
    std::cout << "\n";
}

void Help(const std::string& programName) {
    info();
    std::cout << "Usage Example : " << programName << " -f ./Mal.exe -q\n";
    std::cout << "   -f        Path to PE or ShellCode file to Emulate .\n";
    std::cout << "   -s        Number of Steps Limit if 0 then it's Unlimited - (default = 2000000) ,\n";
    std::cout << "             But it works different with Quick Mode - it will increment ,\n";
    std::cout << "             On any branch like call jmp jz ret etc.. , so use smaller value .\n\n";
    std::cout << "   -q        Quick Mode to make Execution Faster But no disasm,\n";
    std::cout << "             [x] In Quick Mode AddressHooks will not work\n\n";
    std::cout << "   -asm      Show Assembly instructions .\n";
    std::cout << "   -x64      By default Cmulator Detect the PE Mode But this one for x64 ShellCodes .\n";
    std::cout << "   -sc       To Notify Cmulator that the File is ShellCode .\n";
    std::cout << "   -ex       show SEH Exceptions Address and Handlers\n\n";
    std::cout << "   -v        Show Some Info When Calling an API and Some Other Stuff .\n";
    std::cout << "   -vv       Like -v But with more info .\n";
    std::cout << "   -vvv      Like -vv But with much much more more info :D - use at your own risk :P .\n\n\n";
}

void LoadConfig() {
    std::ifstream configFile("./config.json");
    if (configFile.is_open()) {
        std::string data((std::istreambuf_iterator<char>(configFile)), std::istreambuf_iterator<char>());
        
        // nlohmann::json JSON = nlohmann::json::parse(data);
        // win32_dir = std::u16string(JSON["system.win32"].get<std::string>().begin(), JSON["system.win32"].get<std::string>().end());
        // win64_dir = std::u16string(JSON["system.win64"].get<std::string>().begin(), JSON["system.win64"].get<std::string>().end());
        // JSAPI = JSON["JS.main"].get<std::string>();
        // ApiSetSchemaPath = std::u16string(JSON["system.Apiset"].get<std::string>().begin(), JSON["system.Apiset"].get<std::string>().end());
        
        // Add path existence checks...
    } else {
        std::cerr << "Config file not found !\n";
        exit(1);
    }
}

int main(int argc, char* argv[]) {
    NormVideo();

    std::string FilePath = "";
    bool IsShellcode = false;
    bool SCx64 = false;

    LoadConfig();

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    if (argc == 1) {
        std::cout << "\n";
        Help(argv[0]);
        exit(0);
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        std::transform(arg.begin(), arg.end(), arg.begin(), ::tolower);

        if (arg == "-h") {
            std::cout << "\n";
            Help(argv[0]);
            exit(0);
        } else if (arg == "-ex") {
            VerboseExcp = true;
        } else if (arg == "-x64") {
            SCx64 = true;
        } else if (arg == "-sc") {
            IsShellcode = true;
        } else if (arg == "-f") {
            if (i + 1 < argc) {
                FilePath = argv[++i];
                // if (!FileExists(FilePath)) { ... }
            }
        } else if (arg == "-s") {
            if (i + 1 < argc) {
                try {
                    Steps_limit = std::stoull(argv[++i]);
                } catch (...) {
                    std::cerr << "[x] Please Enter Steps as number ! - Ex: -s 1000\n";
                    exit(0);
                }
            }
        } else if (arg == "-v") {
            Verbose = true;
        } else if (arg == "-vv") {
            Verbose = true;
            VerboseEx = true;
        } else if (arg == "-vvv") {
            Verbose = true;
            VerboseEx = true;
            VerboseExx = true;
        } else if (arg == "-asm") {
            ShowASM = true;
        } else if (arg == "-q") {
            Speed = true;
        }
    }

    info();

    if (Speed && ShowASM) {
        TextColor(LightRed);
        std::cout << "Can't Use Quick Mode with ASM Mode\n\n";
        NormVideo();
        exit(0);
    }

    srand((unsigned)time(NULL)); // Randomize

    Emulator = new TEmu(FilePath, IsShellcode, SCx64);
    Emulator->Start();

    std::cout << "\n\n";
#ifdef _WIN32
    std::cout << "I just finished ¯\\_(0.0)_/¯\n";
#else
    std::cout << "I just finished ¯\\_(\xE3\x83\x84)_/¯\n"; // UTF-8 for ツ
#endif

    delete Emulator;
    return 0;
}

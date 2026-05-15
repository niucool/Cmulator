/**
 * Cmulator - TEmu constructor/destructor (port of Core/emu.pas TEmu.Create/Destroy)
 */

#include "Emu.h"
#include "Globals.h"
#include "Utils.h"

#include <plog/Log.h>
#include <nlohmann/json.hpp>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <algorithm>

using json = nlohmann::json;

static void LoadApiSetSchema(FastHashMap<std::string, TApiRed>& schema) {
    if (ApiSetSchemaPath.empty()) return;

    std::string path(ApiSetSchemaPath.begin(), ApiSetSchemaPath.end());
    std::ifstream f(path);
    if (!f.is_open()) { PLOG_WARNING << "Apiset not found: " << path; return; }

    json root = json::parse(f);

    auto load = [&](const char* key, bool isWin10) {
        if (!root.contains(key)) return;
        for (auto& item : root[key]) {
            TApiRed r;
            if (isWin10) {
                if (item.contains("red") && item["red"].is_array() && item["red"].size() >= 2) {
                    r.first = item["red"][0].get<std::string>();
                    r.last  = item["red"][1].get<std::string>();
                }
                r._alias = item.value("alias", "");
            } else {
                r.first = item.value("red.F", item.value("first", ""));
                r.last  = item.value("red.L", item.value("last", ""));
            }
            r.count = (uint8_t)item.value("count", 0);
            std::string n = item.value("name", "");
            std::transform(n.begin(), n.end(), n.begin(), ::tolower);
            if (!n.empty()) { schema[n] = r; schema[SplitReg(n)] = r; }
        }
    };
    load("WIN7_APIS", false);
    load("WIN10_APIS", true);
    PLOG_INFO << "ApiSet loaded: " << schema.size() << " entries";
}

TEmu::TEmu(const std::string& _FilePath, bool _Shellcode, bool SCx64) {
    LastGoodPC  = 0;
    SEH_Handler = 0;
    IsException = false;
    isShellCode = _Shellcode;
    FilePath    = _FilePath;
    Stop_Emu    = false;

    LoadApiSetSchema(ApiSetSchema);

    if (isShellCode)
        FilePath = "./shellcode/" + std::string(SCx64 ? "sc64.exe" : "sc32.exe");

    FILE* tf = fopen(FilePath.c_str(), "rb");
    if (!tf) { PLOG_ERROR << "File not found: " << FilePath; exit(1); }
    fclose(tf);

    if (!img.LoadFromFile(FilePath)) {
        PLOG_ERROR << "Not a valid PE: " << FilePath; exit(1);
    }

    Is_x64 = img.Is64bit;
    if (SCx64) Is_x64 = true;
    img.ImageWordSize = Is_x64 ? 8 : 4;

    printf("\"%s\" is : %s\n", img.FileName.c_str(), Is_x64 ? "x64" : "x86");
    printf("Mapping the File ..\n\n");

    if (Is_x64)
        DLL_BASE_LOAD = 0x0000000700000000ULL;
    else
        DLL_BASE_LOAD = 0x0000000070000000ULL;
    DLL_NEXT_LOAD = DLL_BASE_LOAD;

    auto cpuMode = Is_x64 ? UC_MODE_64 : UC_MODE_32;
    err = uc_open(UC_ARCH_X86, cpuMode, &uc);
    if (err != UC_ERR_OK) { PLOG_ERROR << "Unicorn failed: " << uc_strerror(err); exit(1); }
    printf("[+] Unicorn Init done.\n");
}

TEmu::~TEmu() {
    Stop_Emu = true;
    if (uc) { uc_close(uc); uc = nullptr; }
}

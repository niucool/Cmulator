#include "Globals.h"
#include "Emu.h"
#include "js_engine.h"

#include <plog/Log.h>
#include <plog/Initializers/RollingFileInitializer.h>
#include <nlohmann/json.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>

using json = nlohmann::json;

std::wstring s2w(const std::string& str) {
    if (str.empty()) return L"";
    size_t n = mbstowcs(nullptr, str.c_str(), 0);
    if (n == (size_t)-1) return L"";
    std::wstring ws(n, 0);
    mbstowcs(&ws[0], str.c_str(), n);
    return ws;
}

static bool LoadConfig() {
    std::ifstream f("config.json");
    if (!f.is_open()) { PLOG_ERROR << "Config not found"; return false; }
    json cfg = json::parse(f);
    JSAPI = cfg["JS"]["main"].get<std::string>();
    win32_dir = s2w(cfg["system"]["win32"].get<std::string>());
    win64_dir = s2w(cfg["system"]["win64"].get<std::string>());
    ApiSetSchemaPath = s2w(cfg["system"]["Apiset"].get<std::string>());
    return true;
}

static void PrintHelp() {
    puts("\nCmulator v0.4 - x86/x64 RE Sandbox Emulator\n");
    puts("Usage: cmulator -f <file> [options]");
    puts("  -f  PE or shellcode file");
    puts("  -q  Quick mode");
    puts("  -asm Show assembly");
    puts("  -x64 Shellcode is x64");
    puts("  -sc  File is shellcode");
    puts("  -v/-vv  Verbose");
    puts("  -h  Help");
}

int main(int argc, char* argv[]) {
    plog::init(plog::debug, "cmulator.log");

    if (argc < 2) { PrintHelp(); return 0; }

    std::string filePath;
    bool isShellcode = false, scX64 = false;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h")) { PrintHelp(); return 0; }
        if (!strcmp(argv[i], "-v")) Verbose = true;
        if (!strcmp(argv[i], "-vv")) { Verbose = true; VerboseEx = true; }
        if (!strcmp(argv[i], "-vvv")) { Verbose = VerboseEx = VerboseExx = true; }
        if (!strcmp(argv[i], "-q"))  Speed = true;
        if (!strcmp(argv[i], "-asm")) ShowASM = true;
        if (!strcmp(argv[i], "-x64")) scX64 = true;
        if (!strcmp(argv[i], "-sc")) isShellcode = true;
        if (!strcmp(argv[i], "-f") && i + 1 < argc) filePath = argv[++i];
    }

    if (filePath.empty()) { PLOG_ERROR << "No file (-f)"; return 1; }
    if (!LoadConfig()) return 1;

    if (isShellcode) { PLOG_ERROR << "Shellcode not yet ported"; return 1; }

    // Create emulator — Pascal TEmu.Create handles PE load, uc_open, ApiSet
    Emulator = new TEmu(filePath, isShellcode, scX64);

    printf("\n=== Cmulator v0.4 ===\n");
    printf("ImageBase: 0x%llX\n", (unsigned long long)Emulator->img.ImageBase);
    printf("EntryPoint: 0x%llX\n", (unsigned long long)Emulator->img.EntryPointRVA);
    printf("Imports: %zu DLLs\n\n", Emulator->img.Imports.size());

    Emulator->Start();

    Uninit_JSEngine();
    delete Emulator;
    Emulator = nullptr;
    return 0;
}

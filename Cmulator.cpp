/**
 * Cmulator - Main entry point (C++ port of Cmulator.pas)
 * CLI argument parsing, config loading, emulation loop.
 */

#include "globals.h"
#include "emu.h"
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

    // 1. Calculate the required wide character buffer size
    size_t size_needed = mbstowcs(nullptr, str.c_str(), 0);
    if (size_needed == static_cast<size_t>(-1)) return L""; // Invalid sequence

    // 2. Allocate the wide string buffer
    std::wstring wstrTo(size_needed, 0);

    // 3. Perform the actual conversion
    mbstowcs(&wstrTo[0], str.c_str(), size_needed);

    return wstrTo;
}

// ── Load config.json ─────────────────────────────────────────────

static bool LoadConfig() {
    std::ifstream f("config.json");
    if (!f.is_open()) {
        PLOG_ERROR << "Config file not found!";
        return false;
    }

    json cfg = json::parse(f);

    JSAPI = cfg["JS"]["main"].get<std::string>();
    win32_dir = s2w(cfg["system"]["win32"].get<std::string>());
    win64_dir = s2w(cfg["system"]["win64"].get<std::string>());
    ApiSetSchemaPath = s2w(cfg["system"]["Apiset"].get<std::string>());

    // TODO: validate file/dir existence
    return true;
}

// ── Help ─────────────────────────────────────────────────────────

static void PrintHelp() {
    puts("\nCmulator v0.4 - x86/x64 RE Sandbox Emulator\n");
    puts("Usage: cmulator -f <file> [options]");
    puts("  -f        PE or shellcode file to emulate");
    puts("  -s        Step limit (0 = unlimited, default 4000000)");
    puts("  -q        Quick mode (no disasm)");
    puts("  -asm      Show assembly");
    puts("  -x64      Shellcode is x64");
    puts("  -sc       File is shellcode (not PE)");
    puts("  -v        Verbose");
    puts("  -vv       More verbose");
    puts("  -h        This help");
}

// ── Main ─────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    plog::init(plog::debug, "cmulator.log");
    PLOG_INFO << "Cmulator v0.4 starting";

    if (argc < 2) { PrintHelp(); return 0; }

    std::string filePath;
    uint64_t stepsLimit = 4000000;
    bool isShellcode = false;
    bool scX64 = false;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h")) { PrintHelp(); return 0; }
        if (!strcmp(argv[i], "-v")) Verbose = true;
        if (!strcmp(argv[i], "-vv")) { Verbose = true; VerboseEx = true; }
        if (!strcmp(argv[i], "-vvv")) { Verbose = true; VerboseEx = true; VerboseExx = true; }
        if (!strcmp(argv[i], "-q")) Speed = true;
        if (!strcmp(argv[i], "-asm")) ShowASM = true;
        if (!strcmp(argv[i], "-x64")) scX64 = true;
        if (!strcmp(argv[i], "-sc")) isShellcode = true;
        if (!strcmp(argv[i], "-f") && i + 1 < argc) filePath = argv[++i];
        if (!strcmp(argv[i], "-s") && i + 1 < argc) stepsLimit = strtoull(argv[++i], nullptr, 0);
    }

    if (filePath.empty()) {
        PLOG_ERROR << "No input file specified (-f)";
        return 1;
    }

    // Load config
    if (!LoadConfig()) return 1;

    if (isShellcode) {
        PLOG_ERROR << "Shellcode mode is not ported yet";
        return 1;
    }

    Steps_limit = stepsLimit;
    Steps = 0;

    // Create emulator
    Emulator = new TEmu();
    Emulator->IsSC = isShellcode;

    // Parse PE
    if (!Emulator->img.LoadFromFile(filePath)) {
        PLOG_ERROR << "Failed to load PE: " << filePath;
        delete Emulator;
        Emulator = nullptr;
        return 1;
    }

    Emulator->isx64 = Emulator->img.Is64bit;
    if (scX64) Emulator->isx64 = true;
    Emulator->img.ImageWordSize = Emulator->isx64 ? 8 : 4;

    printf("\n=== Cmulator v0.4 ===\n");
    printf("File: %s (%s)\n", filePath.c_str(), Emulator->isx64 ? "x64" : "x86");
    printf("ImageBase: 0x%llX\n", (unsigned long long)Emulator->img.ImageBase);
    printf("EntryPoint: 0x%llX\n", (unsigned long long)Emulator->img.EntryPointRVA);
    printf("Imports: %zu DLLs\n\n", Emulator->img.Imports.size());

    // Init Unicorn; TEmu::Start owns mapping, segments, hooks, DLL init and run.
    uc_err err = uc_open(UC_ARCH_X86,
        Emulator->isx64 ? UC_MODE_64 : UC_MODE_32,
        &Emulator->uc);
    if (err != UC_ERR_OK) {
        PLOG_ERROR << "uc_open failed: " << uc_strerror(err);
        delete Emulator;
        Emulator = nullptr;
        return 1;
    }

    Emulator->Start();

    // Cleanup
    Uninit_JSEngine();
    uc_close(Emulator->uc);
    delete Emulator;
    Emulator = nullptr;
    return 0;
}

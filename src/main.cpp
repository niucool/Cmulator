/**
 * Cmulator - Main entry point (C++ port of Cmulator.pas)
 * CLI argument parsing, config loading, emulation loop.
 */

#include "globals.h"
#include "emu.h"
#include "pe_image.h"
#include "pe_loader.h"
#include "segments.h"
#include "utils.h"
#include "js_engine.h"

#include <plog/Log.h>
#include <plog/Initializers/RollingFileInitializer.h>
#include <nlohmann/json.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>

using json = nlohmann::json;

// ── Load config.json ─────────────────────────────────────────────

static bool LoadConfig() {
    std::ifstream f("config.json");
    if (!f.is_open()) {
        PLOG_ERROR << "Config file not found!";
        return false;
    }

    json cfg = json::parse(f);

    win32_path = cfg["system"]["win32"].get<std::wstring>();
    win64_path = cfg["system"]["win64"].get<std::wstring>();
    JSAPI_path  = cfg["JS"]["main"].get<std::string>();
    ApiSetSchemaPath = cfg["system"]["Apiset"].get<std::wstring>();

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

    // Create emulator
    Emulator = new TEmu();

    // Parse PE
    if (!Emulator->img.LoadFromFile(filePath)) {
        PLOG_ERROR << "Failed to load PE: " << filePath;
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

    // Init Unicorn
    uc_err err = uc_open(UC_ARCH_X86,
        Emulator->isx64 ? UC_MODE_64 : UC_MODE_32,
        &Emulator->uc);
    if (err != UC_ERR_OK) {
        PLOG_ERROR << "uc_open failed: " << uc_strerror(err);
        return 1;
    }

    // Map PE to Unicorn memory
    size_t mapSize = Emulator->img.SizeOfImage;
    err = uc_mem_map(Emulator->uc, Emulator->img.ImageBase, mapSize, UC_PROT_ALL);
    if (err != UC_ERR_OK) {
        PLOG_ERROR << "uc_mem_map failed: " << uc_strerror(err);
        return 1;
    }

    // Load PE file bytes into Unicorn memory
    FILE* fp = fopen(filePath.c_str(), "rb");
    if (fp) {
        fseek(fp, 0, SEEK_END);
        long fsize = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        std::vector<uint8_t> fbuf(fsize);
        fread(fbuf.data(), 1, fsize, fp);
        fclose(fp);
        uc_mem_write(Emulator->uc, Emulator->img.ImageBase, fbuf.data(), fsize);
    }

    // Fix imports
    HookImports(Emulator->uc, Emulator->img);

    // Init QuickJS
    Init_QJS();
    if (!JSAPI_path.empty()) {
        LoadScript(JSAPI_path.c_str());
    }
    InitJSEmu();

    // Run
    printf("Starting emulation...\n\n");
    uint64_t entryVA = Emulator->img.ImageBase + Emulator->img.EntryPointRVA;
    err = uc_emu_start(Emulator->uc, entryVA, 0, 0, 0);
    if (err != UC_ERR_OK) {
        printf("Emulation ended: %s\n", uc_strerror(err));
    }

    // Cleanup
    Uninit_JSEngine();
    uc_close(Emulator->uc);
    delete Emulator;
    return 0;
}

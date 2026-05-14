/**
 * Cmulator - PE Image wrapper (adapts pe-parse → TPEImage-compatible interface)
 */

#pragma once

#include "types.h"
#include "struct.h"

#include <pe-parse/parse.h>

#include <cstdint>
#include <string>
#include <vector>

struct PEImportEntry {
    AnsiString funcName;
    uint64_t   iatRva    = 0;
    bool       isOrdinal = false;
};

struct PEImportLib {
    AnsiString dllName;
    std::vector<PEImportEntry> functions;
    uint64_t iatRva = 0;
};

struct PESection {
    AnsiString name;
    uint64_t   virtualAddress = 0;
    uint32_t   virtualSize    = 0;
};

struct PEImage {
    // ── Pascal-compatible field names ─────────────────────────────
    AnsiString FileName;
    uint64_t   ImageBase     = 0;
    uint64_t   SizeOfImage   = 0;
    uint32_t   ImageWordSize = 4;    // 4 (x32) or 8 (x64)
    uint64_t   EntryPointRVA = 0;
    uint16_t   Machine       = 0;
    bool       Is64bit       = false;

    std::vector<PEImportLib> Imports;
    std::vector<PESection>   Sections;
    std::vector<uint64_t>    TlsCallbacks;

    // ── Internal pe-parse handle ──────────────────────────────────
    peparse::parsed_pe* _pe = nullptr;

    bool LoadFromFile(const std::string& path);
    void Close();

    ~PEImage() { Close(); }
};

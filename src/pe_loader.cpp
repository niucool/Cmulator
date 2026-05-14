/**
 * Cmulator - PE Loader implementation (C++ port of Core/pe_loader.pas)
 */

#include "pe_loader.h"
#include "globals.h"
#include "emu.h"
#include "utils.h"

#include <plog/Log.h>
#include <xxhash.h>

#include <algorithm>
#include <cstdio>

// ── Unicode → narrow conversion helper ──────────────────────────

static AnsiString w2a(const UnicodeString& w) {
    return AnsiString(w.begin(), w.end());
}

// ── GetModulesCount ──────────────────────────────────────────────

int GetModulesCount(const FastHashMap<std::string, TNewDll>& libs) {
    int count = 0;
    for (auto& [name, lib] : libs)
        if (name.find("api-ms-win") != 0) count++;
    return count;
}

// ── load_sys_dll ────────────────────────────────────────────────

bool load_sys_dll(uc_engine* uc, const std::string& dll) {
    if (!Emulator || dll.empty()) return false;

    // Normalize DLL name
    std::string libName = dll;
    std::transform(libName.begin(), libName.end(), libName.begin(), ::tolower);

    if (libName.find("ms-") != std::string::npos)
        libName = w2a(GetDllFromApiSet(libName));

    libName = ExtractFileNameWithoutExt(libName) + ".dll";
    std::transform(libName.begin(), libName.end(), libName.begin(), ::tolower);

    if (Emulator->Libs.find(libName) != Emulator->Libs.end()) {
        PLOG_DEBUG << "Already loaded: " << libName;
        return true;
    }

    UnicodeString wpath = GetFullPath(libName);
    AnsiString path = w2a(wpath);

    // Check file exists
    FILE* f = _wfopen(wpath.c_str(), L"rb");
    if (!f) {
        PLOG_ERROR << "Library not found: " << path;
        return false;
    }
    fclose(f);

    // Parse DLL
    PEImage sysImg;
    if (!sysImg.LoadFromFile(path)) {
        PLOG_ERROR << "Failed to parse DLL: " << path;
        return false;
    }

    // ── Relocate to new base ─────────────────────────────────────
    uint64_t base = (Emulator->DLL_NEXT_LOAD + UC_PAGE_SIZE * 2 - 1)
                  & ~(UC_PAGE_SIZE * 2 - 1);

    uc_err err = uc_mem_map(uc, base, static_cast<size_t>(sysImg.SizeOfImage), UC_PROT_ALL);
    if (err != UC_ERR_OK) {
        PLOG_ERROR << "uc_mem_map failed: " << uc_strerror(err);
        return false;
    }

    // ── Copy section data via pe-parse iterator ──────────────────
    FILE* dllFile = _wfopen(wpath.c_str(), L"rb");
    if (!dllFile) return false;

    peparse::IterSec(sysImg._pe, [](void* u, const peparse::VA& se,
                       const std::string&, const peparse::image_section_header&,
                       const peparse::bounded_buffer* d) -> int {
        auto* ctx = static_cast<std::pair<uc_engine*,uint64_t>*>(u);
        if (d && d->buf && d->bufLen > 0)
            uc_mem_write(ctx->first, ctx->second + se, d->buf, d->bufLen);
        return 0;
    }, &std::pair<uc_engine*,uint64_t>(uc, base));

    fclose(dllFile);

    // ── Collect exports ─────────────────────────────────────────
    FastHashMap<uint64_t, TLibFunction> byAddr, byOrdinal, byName;

    peparse::IterExpFull(sysImg._pe, [](void* u, const peparse::VA& addr,
                          uint16_t ord, const std::string&,
                          const std::string& fn, const std::string& fwd) -> int {
        auto* ctx = static_cast<std::tuple<decltype(byAddr)*,
                           decltype(byOrdinal)*, decltype(byName)*,
                           std::string*>*>(u);
        auto& ba = *std::get<0>(*ctx);
        auto& bo = *std::get<1>(*ctx);
        auto& bn = *std::get<2>(*ctx);
        auto& ln = *std::get<3>(*ctx);

        uint64_t va = static_cast<uint64_t>(addr);
        bool isFwd = !fwd.empty();
        std::string fName = fn.empty() ? "#" + std::to_string(ord) : fn;

        TLibFunction rec(ln, fName, va, ord, nullptr, isFwd, fn.empty(), fwd);
        ba[va] = rec;

        uint64_t h = XXH64((ExtractFileNameWithoutExt(ln) + "." + std::to_string(ord)).c_str(), 0, 0);
        bo[h] = rec;

        if (!fn.empty()) {
            std::string n = ExtractFileNameWithoutExt(ln) + "." + fn;
            std::transform(n.begin(), n.end(), n.begin(), ::tolower);
            bn[XXH64(n.c_str(), n.size(), 0)] = rec;
        }
        return 0;
    }, &std::tie(byAddr, byOrdinal, byName, libName));

    Emulator->Libs[libName] = TNewDll(base + sysImg.EntryPointRVA,
        libName, base, static_cast<uint32_t>(sysImg.SizeOfImage),
        std::move(byAddr), std::move(byOrdinal), std::move(byName));
    Emulator->DLL_NEXT_LOAD += sysImg.SizeOfImage;

    PLOG_INFO << "Loaded: " << libName << " @ 0x" << std::hex << base;
    return true;
}

// ── HookImports ──────────────────────────────────────────────────

void HookImports(uc_engine* uc, PEImage& img) {
    if (!Emulator) return;

    PLOG_INFO << "Fixing imports for " << img.FileName
              << " (" << img.Imports.size() << " DLLs)";

    for (auto& lib : img.Imports) {
        std::string dllName = ExtractFileNameWithoutExt(lib.dllName) + ".dll";
        std::transform(dllName.begin(), dllName.end(), dllName.begin(), ::tolower);

        if (dllName.find("ms-") != std::string::npos)
            dllName = w2a(GetDllFromApiSet(dllName));

        dllName = ExtractFileNameWithoutExt(dllName) + ".dll";
        std::transform(dllName.begin(), dllName.end(), dllName.begin(), ::tolower);

        if (Emulator->Libs.find(dllName) == Emulator->Libs.end())
            if (!load_sys_dll(uc, dllName)) continue;

        auto it = Emulator->Libs.find(dllName);
        if (it == Emulator->Libs.end()) continue;
        auto& sysDll = it->second;

        uint64_t rva = img.ImageBase + lib.iatRva;
        for (auto& fn : lib.functions) {
            uint64_t funcAddr = 0;
            if (!fn.isOrdinal) {
                std::string hs = ExtractFileNameWithoutExt(dllName) + "." + fn.funcName;
                std::transform(hs.begin(), hs.end(), hs.begin(), ::tolower);
                uint64_t hash = XXH64(hs.c_str(), hs.size(), 0);
                auto fi = sysDll.FnByName.find(hash);
                if (fi != sysDll.FnByName.end()) funcAddr = fi->second.VAddress;
            }
            if (funcAddr == 0)
                funcAddr = GetProcAddr(CmuGetModulehandle(dllName), fn.funcName);
            if (funcAddr != 0)
                uc_mem_write(uc, rva, &funcAddr, img.ImageWordSize);
            rva += img.ImageWordSize;
        }
    }
}

// ── InitDll ──────────────────────────────────────────────────────

void InitDll(uc_engine* uc, TNewDll& lib) {
    if (!Emulator || lib.EntryPoint == 0) return;
    if (lib.Dllname.find("ntdll") == 0 || lib.Dllname.find("crypt32") == 0) return;

    uint64_t rsp = (Emulator->stack_base + Emulator->stack_size) - 0x100;
    uc_reg_write(uc, UC_X86_REG_ESP, &rsp);
    push_value(0); push_value(1);
    push_value(lib.BaseAddress);
    push_value(0xDEADC0DEULL);
    PLOG_DEBUG << "InitDll: " << lib.Dllname;
}

// ── Init_dlls ────────────────────────────────────────────────────

void Init_dlls() {
    if (!Emulator) return;
    for (auto& [n, lib] : Emulator->Libs)
        if (n.find("api-ms-win") != 0) InitDll(Emulator->uc, lib);
}

// ── InitTLS ─────────────────────────────────────────────────────

void InitTLS(uc_engine* uc, PEImage& img) {
    if (!Emulator || img.TlsCallbacks.empty()) return;
    for (auto cb : img.TlsCallbacks) {
        uint64_t rsp = (Emulator->stack_base + Emulator->stack_size) - 0x100;
        uc_reg_write(uc, UC_X86_REG_ESP, &rsp);
        push_value(0); push_value(1);
        push_value(img.ImageBase);
        push_value(0xDEADC0DEULL);
    }
}

// ── MapToMemory ──────────────────────────────────────────────────

MemoryStream MapToMemory(PEImage& pe) {
    MemoryStream result;
    result.data.resize(static_cast<size_t>(pe.SizeOfImage), 0);
    // TODO: copy raw file data into result
    return result;
}

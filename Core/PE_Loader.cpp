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

    std::pair<uc_engine*, uint64_t> sectionCtx{uc, base};
    peparse::IterSec(sysImg._pe, [](void* u, const peparse::VA& se,
                        const std::string&, const peparse::image_section_header&,
                        const peparse::bounded_buffer* d) -> int {
        auto* ctx = static_cast<std::pair<uc_engine*,uint64_t>*>(u);
        if (d && d->buf && d->bufLen > 0)
            uc_mem_write(ctx->first, ctx->second + se, d->buf, d->bufLen);
        return 0;
    }, &sectionCtx);

    fclose(dllFile);

    // ── Collect exports ─────────────────────────────────────────
    FastHashMap<uint64_t, TLibFunction> byAddr, byOrdinal, byName;

    struct ExportCtx {
        FastHashMap<uint64_t, TLibFunction>* byAddr;
        FastHashMap<uint64_t, TLibFunction>* byOrdinal;
        FastHashMap<uint64_t, TLibFunction>* byName;
        std::string* libName;
    } exportCtx{&byAddr, &byOrdinal, &byName, &libName};

    peparse::IterExpFull(sysImg._pe, [](void* u, const peparse::VA& addr,
                           uint16_t ord, const std::string&,
                           const std::string& fn, const std::string& fwd) -> int {
        auto* ctx = static_cast<ExportCtx*>(u);
        auto& ba = *ctx->byAddr;
        auto& bo = *ctx->byOrdinal;
        auto& bn = *ctx->byName;
        auto& ln = *ctx->libName;

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
    }, &exportCtx);

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

    printf("\n[*] Init %zu TLS Callbacks\n", img.TlsCallbacks.size());
    for (auto cbRva : img.TlsCallbacks) {
        uint64_t sp = (Emulator->stack_base + Emulator->stack_size) - 0x100;
        int spReg = Emulator->isx64 ? UC_X86_REG_RSP : UC_X86_REG_ESP;
        uc_reg_write(uc, spReg, &sp);

        push_value(0);             // lpReserved
        push_value(1);             // DLL_PROCESS_ATTACH
        push_value(img.ImageBase); // DllHandle
        push_value(0xDEADC0DEULL); // synthetic return address

        uint64_t callbackVA = img.ImageBase + cbRva;
        PLOG_DEBUG << "Call TLS callback: 0x" << std::hex << callbackVA;
        Emulator->ResetEFLAGS();
        if (Emulator->SaveCPUState()) {
            Emulator->err = uc_emu_start(uc, callbackVA, 0, MICROSECONDS * 10, 0);
            if (!Emulator->RestoreCPUState()) {
                PLOG_ERROR << "RestoreCPUState failed after TLS callback";
                return;
            }
        } else {
            PLOG_ERROR << "SaveCPUState failed before TLS callback";
            return;
        }
    }
    printf("[+] Init TLS Callbacks done\n");
}
// ── MapToMemory ──────────────────────────────────────────────────

MemoryStream MapToMemory(PEImage& pe) {
    MemoryStream result;
    result.data.resize(static_cast<size_t>(pe.SizeOfImage), 0);
    // TODO: copy raw file data into result
    return result;
}

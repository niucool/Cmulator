#include "ethreads.h"
#include "globals.h"
#include "emu.h"
#include "utils.h"

#include <plog/Log.h>
#include <algorithm>
#include <cstring>
#include <vector>

// Pascal-compatible structure sizes
constexpr uint32_t PEB_LDR_DATA_32_SIZE = 0x28;
constexpr uint32_t PEB_LDR_DATA_64_SIZE = 0x58;

// ── Helpers ──────────────────────────────────────────────────────

static bool SortByBaseAddr(const std::pair<std::string, TNewDll>& a,
                           const std::pair<std::string, TNewDll>& b) {
    return a.second.BaseAddress < b.second.BaseAddress;
}

int GetModulesCount_Excluded(const FastHashMap<std::string, TNewDll>& libs) {
    int count = 0;
    for (auto& [name, lib] : libs)
        if (name.find("api-ms-win") != 0) count++;
    return count;
}

// ── BuildPEB_Ldr_Entry32 ─────────────────────────────────────────

static uint64_t g_EntryNextOffset = 0;

LdrDataTableEntry32 BuildPEB_Ldr_Entry32(uc_engine* uc, uint64_t offset,
    uint64_t blink, const TNewDll& lib, const std::string& main_path, bool isEnd) {

    std::string path = main_path + lib.Dllname;
    uint32_t FPath_len  = static_cast<uint32_t>(path.length() * 2);
    uint32_t Path_len   = static_cast<uint32_t>(main_path.length() * 2);
    uint32_t name_len   = static_cast<uint32_t>(lib.Dllname.length() * 2);

    uint32_t pre_len = sizeof(LdrDataTableEntry32) + FPath_len;
    uint32_t _flink32 = static_cast<uint32_t>(g_EntryNextOffset + pre_len + 2);

    if (isEnd)
        _flink32 = static_cast<uint32_t>(offset);

    LdrDataTableEntry32 Entry{};
    Entry.InLoadOrderLinks.Flink = _flink32;
    Entry.InLoadOrderLinks.Blink = static_cast<uint32_t>(blink);
    Entry.InMemoryOrderLinks.Flink = _flink32 + 8;
    Entry.InMemoryOrderLinks.Blink = static_cast<uint32_t>(blink) + 8;
    Entry.InInitializationOrderLinks.Flink = _flink32 + 16;
    Entry.InInitializationOrderLinks.Blink = static_cast<uint32_t>(blink) + 16;
    Entry.DllBase   = static_cast<uint32_t>(lib.BaseAddress);
    Entry.EntryPoint = static_cast<uint32_t>(lib.EntryPoint);
    Entry.SizeOfImage = lib.ImageSize;
    Entry.FullDllName.Length = FPath_len;
    Entry.FullDllName.MaximumLength = FPath_len + 2;
    Entry.FullDllName.Buffer = static_cast<uint32_t>(g_EntryNextOffset + sizeof(LdrDataTableEntry32));
    Entry.BaseDllName.Length = name_len;
    Entry.BaseDllName.MaximumLength = name_len + 2;
    Entry.BaseDllName.Buffer = static_cast<uint32_t>(g_EntryNextOffset + sizeof(LdrDataTableEntry32) + Path_len);

    if (Emulator)
        Emulator->err = uc_mem_write(uc, g_EntryNextOffset, &Entry, sizeof(Entry));
    WriteStringW(g_EntryNextOffset + sizeof(Entry), path);

    return Entry;
}

// ── BuildPEB_Ldr_Entry64 ─────────────────────────────────────────

LdrDataTableEntry64 BuildPEB_Ldr_Entry64(uc_engine* uc, uint64_t offset,
    uint64_t blink, const TNewDll& lib, const std::string& main_path, bool isEnd) {

    std::string path = main_path + lib.Dllname;
    uint32_t FPath_len  = static_cast<uint32_t>(path.length() * 2);
    uint32_t Path_len   = static_cast<uint32_t>(main_path.length() * 2);
    uint32_t name_len   = static_cast<uint32_t>(lib.Dllname.length() * 2);

    uint32_t pre_len = sizeof(LdrDataTableEntry64) + FPath_len;
    uint64_t _flink = g_EntryNextOffset + pre_len + 2;

    if (isEnd)
        _flink = offset;

    LdrDataTableEntry64 Entry{};
    Entry.InLoadOrderLinks.Flink = _flink;
    Entry.InLoadOrderLinks.Blink = blink;
    Entry.InMemoryOrderLinks.Flink = _flink + 16;
    Entry.InMemoryOrderLinks.Blink = blink + 16;
    Entry.InInitializationOrderLinks.Flink = _flink + 32;
    Entry.InInitializationOrderLinks.Blink = blink + 32;
    Entry.DllBase   = lib.BaseAddress;
    Entry.EntryPoint = lib.EntryPoint;
    Entry.SizeOfImage = lib.ImageSize;
    Entry.FullDllName.Length = FPath_len;
    Entry.FullDllName.MaximumLength = FPath_len + 2;
    Entry.FullDllName.Buffer = g_EntryNextOffset + sizeof(LdrDataTableEntry64);
    Entry.BaseDllName.Length = name_len;
    Entry.BaseDllName.MaximumLength = name_len + 2;
    Entry.BaseDllName.Buffer = g_EntryNextOffset + sizeof(LdrDataTableEntry64) + Path_len;

    if (Emulator)
        Emulator->err = uc_mem_write(uc, g_EntryNextOffset, &Entry, sizeof(Entry));
    WriteStringW(g_EntryNextOffset + sizeof(Entry), path);

    return Entry;
}

// ── BuildPEB_Ldr ─────────────────────────────────────────────────

void BuildPEB_Ldr(uc_engine* uc, uint64_t offset, bool x64) {
    if (!Emulator) return;

    int index = 0;
    bool is_end = false;
    const std::string sys_path = "C:\\Windows\\System32\\";

    if (x64) {
        uint64_t flink64 = offset + PEB_LDR_DATA_64_SIZE;
        uint64_t blink64 = 0;
        uint64_t start_of_list = offset + 0x18;
        uint64_t end_of_list = 0;

        // Initialize PEB_LDR_DATA_64
        PebLdrData64 LDR_DATA64{};
        LDR_DATA64.Length = PEB_LDR_DATA_64_SIZE;
        LDR_DATA64.InLoadOrderModuleList.Flink = flink64;
        LDR_DATA64.InMemoryOrderModuleList.Flink = flink64 + 16;
        LDR_DATA64.InInitializationOrderModuleList.Flink = flink64 + 32;

        // Add main module (self)
        blink64 = start_of_list;
        TNewDll MainModule;
        MainModule.EntryPoint   = Emulator->img.ImageBase + Emulator->img.EntryPointRVA;
        MainModule.BaseAddress  = Emulator->img.ImageBase;
        MainModule.Dllname      = Emulator->img.FileName;
        // Extract just the filename
        auto sep = MainModule.Dllname.find_last_of("/\\");
        if (sep != std::string::npos) MainModule.Dllname = MainModule.Dllname.substr(sep + 1);
        MainModule.ImageSize    = Emulator->img.SizeOfImage;

        g_EntryNextOffset = flink64;
        auto module64 = BuildPEB_Ldr_Entry64(uc, flink64, blink64, MainModule,
                            "C:\\Users\\PlaMan\\", false);
        end_of_list = flink64;

        uint64_t NewOffset = sizeof(module64) + module64.FullDllName.MaximumLength;
        blink64 = flink64;
        flink64 += NewOffset;
        g_EntryNextOffset = flink64;

        // Sort libs by base address
        std::vector<std::pair<std::string, TNewDll>> TLibsArray;
        for (auto& kv : Emulator->Libs)
            TLibsArray.push_back(kv);
        std::sort(TLibsArray.begin(), TLibsArray.end(), SortByBaseAddr);

        int ModulesCount = GetModulesCount_Excluded(Emulator->Libs);
        for (auto& TLibItem : TLibsArray) {
            if (TLibItem.second.Dllname.find("api-ms-win") != 0) {
                if (index + 1 == ModulesCount) {
                    end_of_list = flink64;
                    flink64 = start_of_list;
                    is_end = true;
                }

                auto module = BuildPEB_Ldr_Entry64(uc, flink64, blink64,
                    TLibItem.second, sys_path, is_end);
                blink64 = flink64;
                flink64 += sizeof(module) + module.FullDllName.MaximumLength;
                g_EntryNextOffset = flink64;
                index++;
            }
        }

        LDR_DATA64.InLoadOrderModuleList.Blink = end_of_list;
        LDR_DATA64.InMemoryOrderModuleList.Blink = end_of_list + 16;
        LDR_DATA64.InInitializationOrderModuleList.Blink = end_of_list + 32;

        uc_err err = uc_mem_write(uc, offset, &LDR_DATA64, sizeof(PebLdrData64));
        if (err != UC_ERR_OK) {
            PLOG_ERROR << "Error writing Ldr_data to memory";
        }
    } else {
        uint32_t flink32 = static_cast<uint32_t>(offset + PEB_LDR_DATA_32_SIZE);
        uint32_t blink32 = 0;
        uint32_t start_of_list = static_cast<uint32_t>(offset + 0x0C);
        uint32_t end_of_list = 0;

        // Initialize PEB_LDR_DATA_32
        PebLdrData32 LDR_DATA32{};
        LDR_DATA32.Length = PEB_LDR_DATA_32_SIZE;
        LDR_DATA32.InLoadOrderModuleList.Flink = flink32;
        LDR_DATA32.InMemoryOrderModuleList.Flink = flink32 + 8;
        LDR_DATA32.InInitializationOrderModuleList.Flink = flink32 + 16;

        // Add main module (self)
        blink32 = start_of_list;
        TNewDll MainModule;
        MainModule.EntryPoint   = Emulator->img.ImageBase + Emulator->img.EntryPointRVA;
        MainModule.BaseAddress  = Emulator->img.ImageBase;
        MainModule.Dllname      = Emulator->img.FileName;
        auto sep = MainModule.Dllname.find_last_of("/\\");
        if (sep != std::string::npos) MainModule.Dllname = MainModule.Dllname.substr(sep + 1);
        MainModule.ImageSize    = Emulator->img.SizeOfImage;

        g_EntryNextOffset = flink32;
        auto module32 = BuildPEB_Ldr_Entry32(uc, flink32, blink32, MainModule,
                            "C:\\Users\\PlaMan\\", false);
        end_of_list = flink32;

        uint64_t NewOffset = sizeof(module32) + module32.FullDllName.MaximumLength;
        blink32 = flink32;
        flink32 += static_cast<uint32_t>(NewOffset);
        g_EntryNextOffset = flink32;

        // Sort libs by base address
        std::vector<std::pair<std::string, TNewDll>> TLibsArray;
        for (auto& kv : Emulator->Libs)
            TLibsArray.push_back(kv);
        std::sort(TLibsArray.begin(), TLibsArray.end(), SortByBaseAddr);

        int ModulesCount = GetModulesCount_Excluded(Emulator->Libs);
        for (auto& TLibItem : TLibsArray) {
            if (TLibItem.second.Dllname.find("api-ms-win") != 0) {
                if (index + 1 == ModulesCount) {
                    end_of_list = flink32;
                    flink32 = start_of_list;
                    is_end = true;
                }

                auto module = BuildPEB_Ldr_Entry32(uc, flink32, blink32,
                    TLibItem.second, sys_path, is_end);
                blink32 = flink32;
                flink32 += static_cast<uint32_t>(sizeof(module) + module.FullDllName.MaximumLength);
                g_EntryNextOffset = flink32;
                index++;
            }
        }

        LDR_DATA32.InLoadOrderModuleList.Blink = end_of_list;
        LDR_DATA32.InMemoryOrderModuleList.Blink = end_of_list + 8;
        LDR_DATA32.InInitializationOrderModuleList.Blink = end_of_list + 16;

        uc_err err = uc_mem_write(uc, offset, &LDR_DATA32, sizeof(PebLdrData32));
        if (err != UC_ERR_OK) {
            PLOG_ERROR << "Error writing Ldr_data to memory";
        }
    }
}

// ── BuildPEB ─────────────────────────────────────────────────────

void BuildPEB(uc_engine* uc, uint64_t PEB, bool x64) {
    if (!Emulator) return;

    if (x64) {
        Peb64 peb{};
        peb.BeingDebugged    = false;
        peb.ImageBaseAddress = Emulator->img.ImageBase;
        peb.Ldr              = static_cast<int64_t>(PEB + sizeof(Peb64));
        peb.OSMajorVersion   = 10;
        peb.OSMinorVersion   = 0;
        peb.OSBuildNumber    = 10240;
        peb.OSCSDVersion     = 0;
        peb.OSPlatformId     = 0;
        peb.NtGlobalFlag     = 0;

        uc_err err = uc_mem_write(uc, PEB, &peb, sizeof(peb));
        if (err != UC_ERR_OK) {
            PLOG_ERROR << "Error writing PEB to memory";
            return;
        }
        BuildPEB_Ldr(uc, peb.Ldr, x64);
    } else {
        Peb32 peb{};
        peb.BeingDebugged    = false;
        peb.ImageBaseAddress = static_cast<uint32_t>(Emulator->img.ImageBase);
        peb.Ldr              = static_cast<uint32_t>(PEB + sizeof(Peb32));
        peb.OSMajorVersion   = 10;
        peb.OSMinorVersion   = 0;
        peb.OSBuildNumber    = 10240;
        peb.OSCSDVersion     = 0;
        peb.OSPlatformId     = 0;
        peb.NtGlobalFlag     = 0;

        uc_err err = uc_mem_write(uc, PEB, &peb, sizeof(peb));
        if (err != UC_ERR_OK) {
            PLOG_ERROR << "Error writing PEB to memory";
            return;
        }
        BuildPEB_Ldr(uc, peb.Ldr, x64);
    }
}

// ── InitTEB_PEB ──────────────────────────────────────────────────

bool InitTEB_PEB(uc_engine* uc, uint64_t fs, uint64_t gs, uint64_t PEB,
                 uint64_t stack_address, uint64_t stack_limit, bool x64) {
    if (!Emulator) return false;

    if (x64) {
        // Initialize x64 TIB structure directly via field writes
        const uint64_t tebAddr = gs;

        // ExceptionList = -1
        uint64_t neg1 = 0xFFFFFFFFFFFFFFFFULL;
        uc_mem_write(uc, tebAddr + 0x00, &neg1, 8);
        // StackBase
        uc_mem_write(uc, tebAddr + 0x08, &stack_address, 8);
        // StackLimit
        uc_mem_write(uc, tebAddr + 0x10, &stack_limit, 8);
        // SubSystemTib = 0 (already zero)
        // FiberData = 0 (already zero)
        // Self = gs
        uc_mem_write(uc, tebAddr + 0x30, &gs, 8);
        // EnvironmentPointer = 0 (already zero)
        // ClientId (UniqueProcess, UniqueThread)
        uint32_t pid = (rand() % 1000) + 1000;
        uint32_t tid = (rand() % 1000) + 3000;
        uint64_t uniqueProcess = pid;
        uint64_t uniqueThread  = tid;
        uc_mem_write(uc, tebAddr + 0x40, &uniqueProcess, 8);
        uc_mem_write(uc, tebAddr + 0x48, &uniqueThread, 8);
        // ActiveRpcHandle = 0 (already zero)
        // ThreadLocalStoragePointer = stack_address
        uc_mem_write(uc, tebAddr + 0x58, &stack_address, 8);
        // Peb
        uc_mem_write(uc, tebAddr + 0x60, &PEB, 8);

        Emulator->PID = pid;

        // Build PEB
        BuildPEB(uc, PEB, x64);

        // Additional x64 setup (from Pascal: custom stuff)
        uint64_t tmp = stack_address;
        uc_mem_write(uc, tmp, &tmp, 8);
        uc_mem_write(uc, tebAddr + 0x58, &tmp, 8); // TLS pointer → tmp
        tmp += 0x188;
        uc_mem_write(uc, tmp, &stack_address, 8);

    } else {
        // Initialize x86 TIB structure directly via field writes
        const uint64_t tebAddr = fs;

        // ExceptionList = -1 (DWORD)
        uint32_t neg1 = 0xFFFFFFFF;
        uc_mem_write(uc, tebAddr + 0x00, &neg1, 4);
        // StackBase
        uint32_t sb = static_cast<uint32_t>(stack_address);
        uc_mem_write(uc, tebAddr + 0x04, &sb, 4);
        // StackLimit
        uint32_t sl = static_cast<uint32_t>(stack_limit);
        uc_mem_write(uc, tebAddr + 0x08, &sl, 4);
        // SubSystemTib = 0 (already zero)
        // FiberData = 0 (already zero)
        // Self = fs
        uint32_t fs32 = static_cast<uint32_t>(fs);
        uc_mem_write(uc, tebAddr + 0x18, &fs32, 4);
        // EnvironmentPointer = 0 (already zero)
        // ClientId (UniqueProcess, UniqueThread)
        uint32_t pid = (rand() % 1000) + 1000;
        uint32_t tid = (rand() % 1000) + 3000;
        uc_mem_write(uc, tebAddr + 0x20, &pid, 4);
        uc_mem_write(uc, tebAddr + 0x24, &tid, 4);
        // ActiveRpcHandle = 0 (already zero)
        // ThreadLocalStoragePointer = stack_address
        uc_mem_write(uc, tebAddr + 0x2C, &sb, 4);
        // Peb
        uint32_t peb32 = static_cast<uint32_t>(PEB);
        uc_mem_write(uc, tebAddr + 0x30, &peb32, 4);

        Emulator->PID = pid;

        // Build PEB
        BuildPEB(uc, PEB, x64);

        // Additional x86 setup
        uint32_t LS = sb;
        uint32_t tmp = LS + 4;
        uc_mem_write(uc, LS, &tmp, 4);
    }

    return true;
}

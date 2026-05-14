/**
 * Cmulator - Utility functions implementation (C++ port of Core/utils.pas)
 *
 * Dependencies: Emulator (global TEmu*), Zydis, xxHash
 */

#include "utils.h"
#include "globals.h"
#include "emu.h"
#include "fnhook.h"

#include <unicorn/unicorn.h>
#include <Zydis/Zydis.h>
#include <xxhash.h>

#ifdef _WIN32
  #include <windows.h>
  #include <algorithm>  // std::replace
#else
  #include <cstdio>
#endif

#include <cstring>
#include <cwchar>
#include <algorithm>
#include <cinttypes>

// ── Register I/O ──────────────────────────────────────────────────

uint32_t reg_read_x32(uc_engine* uc, int reg) {
    uint32_t result = 0;
    uc_reg_read(uc, reg, &result);
    return result;
}

uint64_t reg_read_x64(uc_engine* uc, int reg) {
    uint64_t result = 0;
    if (Emulator) Emulator->err = uc_reg_read(uc, reg, &result);
    else uc_reg_read(uc, reg, &result);
    return result;
}

bool reg_write_x32(uc_engine* uc, int reg, uint32_t value) {
    return uc_reg_write(uc, reg, &value) == UC_ERR_OK;
}

bool reg_write_x64(uc_engine* uc, int reg, uint64_t value) {
    if (Emulator) Emulator->err = uc_reg_write(uc, reg, &value);
    else uc_reg_write(uc, reg, &value);
    return Emulator ? Emulator->err == UC_ERR_OK : true;
}

// ── Stack operations ──────────────────────────────────────────────

bool push_value(uint64_t value) {
    if (!Emulator) return false;
    uint64_t stack = 0;
    int sp_reg = Emulator->isx64 ? UC_X86_REG_RSP : UC_X86_REG_ESP;
    Emulator->err = uc_reg_read(Emulator->uc, sp_reg, &stack);
    if (Emulator->err != UC_ERR_OK) return false;

    stack -= Emulator->img.ImageWordSize;  // sub SP, {4 or 8}
    Emulator->err = uc_reg_write(Emulator->uc, sp_reg, &stack);
    if (Emulator->err != UC_ERR_OK) return false;

    Emulator->err = uc_mem_write(Emulator->uc, stack, &value,
                                 Emulator->img.ImageWordSize);
    return Emulator->err == UC_ERR_OK;
}

uint64_t pop_value() {
    if (!Emulator) return 0;
    uint64_t result = 0;
    uint64_t stack = 0;
    int sp_reg = Emulator->isx64 ? UC_X86_REG_RSP : UC_X86_REG_ESP;

    Emulator->err = uc_reg_read(Emulator->uc, sp_reg, &stack);
    Emulator->err = uc_mem_read(Emulator->uc, stack, &result,
                                Emulator->img.ImageWordSize);
    if (Emulator->err == UC_ERR_OK) {
        stack += Emulator->img.ImageWordSize;
        Emulator->err = uc_reg_write(Emulator->uc, sp_reg, &stack);
    }
    return result;
}

// ── UTF-8 conversion ─────────────────────────────────────────────

std::string UTF32CharToUTF8(uint32_t u4c) {
    std::string result;
    if (u4c <= 0x7F) {
        result += static_cast<char>(u4c);
    } else if (u4c <= 0x7FF) {
        result += static_cast<char>(0xC0 | ((u4c >> 6) & 0x1F));
        result += static_cast<char>(0x80 | (u4c & 0x3F));
    } else if (u4c <= 0xFFFF) {
        result += static_cast<char>(0xE0 | ((u4c >> 12) & 0x0F));
        result += static_cast<char>(0x80 | ((u4c >> 6) & 0x3F));
        result += static_cast<char>(0x80 | (u4c & 0x3F));
    } else if (u4c <= 0x1FFFFF) {
        result += static_cast<char>(0xF0 | ((u4c >> 18) & 0x07));
        result += static_cast<char>(0x80 | ((u4c >> 12) & 0x3F));
        result += static_cast<char>(0x80 | ((u4c >> 6) & 0x3F));
        result += static_cast<char>(0x80 | (u4c & 0x3F));
    } else {
        u4c = 0xFFFD;
        result += static_cast<char>(0xE0 | ((u4c >> 12) & 0x0F));
        result += static_cast<char>(0x80 | ((u4c >> 6) & 0x3F));
        result += static_cast<char>(0x80 | (u4c & 0x3F));
    }
    return result;
}

// ── String reads from emulated memory ─────────────────────────────

AnsiString ReadStringW(uint64_t addr, uint32_t len) {
    AnsiString result;
    if (!Emulator) return result;

    uint16_t ch = 0;
    uint32_t count = 0;
    if (len == 0) len = MAX_STRING_LEN;

    do {
        Emulator->err = uc_mem_read(Emulator->uc, addr, &ch, 2);
        result += UTF32CharToUTF8(ch);
        count++;
        addr += 2;
    } while (ch != 0 && count < len);

    // Trim trailing spaces (Pascal Trim)
    while (!result.empty() && result.back() == ' ') result.pop_back();
    return result;
}

AnsiString ReadStringA(uint64_t addr, uint32_t len) {
    AnsiString result;
    if (!Emulator) return result;

    uint8_t ch = 0;
    uint32_t count = 0;
    if (len == 0) len = MAX_STRING_LEN;

    do {
        Emulator->err = uc_mem_read(Emulator->uc, addr, &ch, 1);
        result += static_cast<char>(ch);
        count++;
        addr++;
    } while (ch != 0 && count < len);

    while (!result.empty() && result.back() == ' ') result.pop_back();
    return result;
}

// ── String writes to emulated memory ──────────────────────────────

uint32_t WriteStringA(uint64_t addr, const AnsiString& str) {
    if (!Emulator) return 0;
    uint32_t written = 0;
    for (char c : str) {
        uint8_t ch = static_cast<uint8_t>(c);
        Emulator->err = uc_mem_write(Emulator->uc, addr, &ch, 1);
        if (Emulator->err != UC_ERR_OK) break;
        written++;
        addr++;
    }
    return written;
}

uint32_t WriteStringW(uint64_t addr, const AnsiString& str) {
    if (!Emulator) return 0;
    uint32_t written = 0;
    for (char c : str) {
        uint16_t ch = static_cast<uint16_t>(static_cast<unsigned char>(c));
        Emulator->err = uc_mem_write(Emulator->uc, addr, &ch, 2);
        if (Emulator->err != UC_ERR_OK) break;
        written++;
        addr += 2;
    }
    return written;
}

// ── Memory I/O (wrappers around Unicorn) ──────────────────────────

bool WriteMem(uint64_t addr, const void* value, uint32_t len) {
    if (!Emulator) return false;
    Emulator->err = uc_mem_write(Emulator->uc, addr, value, len);
    return Emulator->err == UC_ERR_OK;
}

bool WriteByte(uint64_t addr, uint8_t value) {
    if (!Emulator) return false;
    Emulator->err = uc_mem_write(Emulator->uc, addr, &value, 1);
    return Emulator->err == UC_ERR_OK;
}

bool WriteWord(uint64_t addr, uint16_t value) {
    if (!Emulator) return false;
    Emulator->err = uc_mem_write(Emulator->uc, addr, &value, 2);
    return Emulator->err == UC_ERR_OK;
}

bool WriteDword(uint64_t addr, uint32_t value) {
    if (!Emulator) return false;
    Emulator->err = uc_mem_write(Emulator->uc, addr, &value, 4);
    return Emulator->err == UC_ERR_OK;
}

bool WriteQword(uint64_t addr, uint64_t value) {
    if (!Emulator) return false;
    Emulator->err = uc_mem_write(Emulator->uc, addr, &value, 8);
    return Emulator->err == UC_ERR_OK;
}

ByteArray ReadMem(uint64_t addr, uint32_t len) {
    ByteArray result(len);
    if (!Emulator) return result;
    Emulator->err = uc_mem_read(Emulator->uc, addr, result.data(), len);
    return result;
}

uint8_t ReadByte(uint64_t addr) {
    uint8_t result = 0;
    if (!Emulator) return 0;
    Emulator->err = uc_mem_read(Emulator->uc, addr, &result, 1);
    return result;
}

uint16_t ReadWord(uint64_t addr) {
    uint16_t result = 0;
    if (!Emulator) return 0;
    Emulator->err = uc_mem_read(Emulator->uc, addr, &result, 2);
    return result;
}

int32_t ReadDword(uint64_t addr) {
    int32_t result = 0;
    if (!Emulator) return 0;
    Emulator->err = uc_mem_read(Emulator->uc, addr, &result, sizeof(result));
    return result;
}

int64_t ReadQword(uint64_t addr) {
    int64_t result = 0;
    if (!Emulator) return 0;
    Emulator->err = uc_mem_read(Emulator->uc, addr, &result, sizeof(result));
    return result;
}

// ── Character checks ──────────────────────────────────────────────

bool cmu_isprint(char c) {
    return (c >= ' ') && (c <= '~') && (static_cast<uint8_t>(c) != 0x7F);
}

bool IsStringPrintable(const std::string& str) {
    for (char c : str) {
        if (!cmu_isprint(c)) return false;
    }
    return !str.empty();
}

// ── Hex dump ──────────────────────────────────────────────────────

void HexDump(const uint8_t* mem, int len, uint64_t virtualAddr, int cols) {
    if (!mem || len <= 0) return;
    if (!Emulator || Emulator->RunOnDll) return;

    printf("\n================================ Hex Dump ====================================\n");
    printf("           0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");

    int remainder = len % cols;
    int max = len + (remainder ? (cols - remainder) : 0) - 1;

    for (int i = 0; i <= max; i++) {
        if ((i % cols) == 0) {
            if (virtualAddr != 0)
                printf("%0*llX : ", Emulator->isx64 ? 16 : 8,
                       static_cast<unsigned long long>(virtualAddr + i));
            else
                printf("%0*llX : ", Emulator->isx64 ? 16 : 8,
                       static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(mem + i)));
        }

        unsigned char ch = (i < len) ? mem[i] : 0;
        if (i < len)
            printf("%02X ", ch);
        else
            printf("   ");

        if ((i % cols) == (cols - 1)) {
            printf(" | ");
            for (int j = i - (cols - 1); j <= i; j++) {
                if (j >= len) {
                    printf(" ");
                } else {
                    unsigned char print_ch = mem[j];
                    printf("%c", cmu_isprint(static_cast<char>(print_ch)) ? print_ch : '.');
                }
            }
            printf("\n");
        }
    }
    printf("==============================================================================\n\n");
}

// ── Stack dump ────────────────────────────────────────────────────

void DumpStack(uint64_t addr, uint32_t size) {
    if (!Emulator) return;

    if (addr == 0) {
        int sp_reg = Emulator->isx64 ? UC_X86_REG_RSP : UC_X86_REG_ESP;
        Emulator->err = uc_reg_read(Emulator->uc, sp_reg, &addr);
    }

    const char* fmt = Emulator->isx64 ? "%.16llx : %.16llx | %s\n" : "%.8llx : %.8llx | %s\n";

    printf("============ Mem Dump ==============\n");

    for (uint32_t i = 0; i < size; i++) {
        uint64_t val = 0;
        Emulator->err = uc_mem_read(Emulator->uc,
                                    addr + (i * Emulator->img.ImageWordSize),
                                    &val, Emulator->img.ImageWordSize);

        std::string ascii = ReadStringA(val);
        std::string unicode = ReadStringW(val);
        std::string display;

        if (IsStringPrintable(ascii)) display = ascii;
        else if (IsStringPrintable(unicode)) display = unicode;

        printf(fmt,
               static_cast<unsigned long long>(addr + (i * Emulator->img.ImageWordSize)),
               static_cast<unsigned long long>(val),
               display.c_str());
    }
    printf("====================================\n\n");
}

// ── Console colors ────────────────────────────────────────────────

#ifdef _WIN32
static WORD g_OriginalConsole = 15;
#endif

void TextColor(uint8_t color) {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO info;
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(hConsole, &info);
    g_OriginalConsole = info.wAttributes;

    WORD attr = 7; // default
    switch (color) {
        case LightRed:     attr = 12; break;
        case LightMagenta: attr = 13; break;
        case Magenta:      attr = 5;  break;
        case Yellow:       attr = 14; break;
        case LightCyan:    attr = 11; break;
        case LightBlue:    attr = 9;  break;
    }
    SetConsoleTextAttribute(hConsole, attr);
#else
    switch (color) {
        case LightRed:     printf("\033[1;31m"); break;
        case LightMagenta: printf("\033[1;35m"); break;
        case Magenta:      printf("\033[3;35m"); break;
        case Yellow:       printf("\033[1;33m"); break;
        case LightCyan:    printf("\033[1;36m"); break;
        case LightBlue:    printf("\033[1;34m"); break;
    }
#endif
}

void NormVideo() {
#ifdef _WIN32
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), g_OriginalConsole);
#else
    printf("\033[0m");
#endif
}

// ── Filename helper ───────────────────────────────────────────────

std::string ExtractFileNameWithoutExt(const std::string& filename) {
    size_t pos = filename.rfind('.');
    size_t sep = filename.find_last_of("/\\");
    if (pos != std::string::npos && (sep == std::string::npos || pos > sep)) {
        return filename.substr(0, pos);
    }
    return filename;
}

// ── DLL resolution ────────────────────────────────────────────────

UnicodeString GetFullPath(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    UnicodeString base = Emulator->isx64 ? win64_path : win32_path;

    // Ensure trailing separator
    if (!base.empty() && base.back() != L'\\') base += L'\\';

    std::wstring wname(lower.begin(), lower.end());
    return base + wname;
}

std::string SplitReg(const std::string& str) {
    // "foo-l1" or "api-ms-win-core-xxx-l1-2-0" → strip suffix "-l\d"
    size_t pos = str.rfind("-l");
    if (pos != std::string::npos && pos + 2 < str.size()) {
        if (isdigit(str[pos + 2])) {
            return str.substr(0, pos);
        }
    }
    return str;
}

UnicodeString GetDllFromApiSet(const std::string& name) {
    if (!Emulator) {
        std::wstring wname(name.begin(), name.end());
        return wname;
    }

    std::string dll = ExtractFileNameWithoutExt(name);
    std::string::size_type sep = dll.find_last_of("/\\");
    if (sep != std::string::npos) dll = dll.substr(sep + 1);

    if (Emulator->ApiSetSchema.find(dll) == Emulator->ApiSetSchema.end()) {
        dll = ExtractFileNameWithoutExt(SplitReg(name));
        sep = dll.find_last_of("/\\");
        if (sep != std::string::npos) dll = dll.substr(sep + 1);
    }

    auto it = Emulator->ApiSetSchema.find(dll);
    if (it != Emulator->ApiSetSchema.end()) {
        const TApiRed& api = it->second;
        // Try last, then alias, then first
        UnicodeString path = GetFullPath(api.last);
        // TODO: FileExists check (platform-dependent)
        path = GetFullPath(api._alias);
        path = GetFullPath(api.first);
        return path;
    }

    std::wstring wname(name.begin(), name.end());
    return wname;
}

uint64_t CmuGetModulehandle(const std::string& module) {
    if (!Emulator) return 0;

    // Check if it's the main image
    std::string imgName = ExtractFileNameWithoutExt(Emulator->img.FileName);
    if (module == imgName)
        return Emulator->img.ImageBase;

    // Look up in loaded libraries
    std::string dllName = ExtractFileNameWithoutExt(module) + ".dll";
    std::transform(dllName.begin(), dllName.end(), dllName.begin(), ::tolower);

    auto it = Emulator->Libs.find(dllName);
    if (it != Emulator->Libs.end())
        return it->second.BaseAddress;

    return 0;
}

uint64_t GetProcAddr(uint64_t handle, const std::string& fnName) {
    if (!Emulator) return 0;

    for (auto& [key, lib] : Emulator->Libs) {
        if (lib.BaseAddress == handle) {
            std::string dllBase = ExtractFileNameWithoutExt(lib.Dllname);
            std::transform(dllBase.begin(), dllBase.end(), dllBase.begin(), ::tolower);

            uint64_t hash = XXH64((dllBase + "." + fnName).c_str(),
                                  dllBase.size() + 1 + fnName.size(), 0);

            auto it = lib.FnByName.find(hash);
            if (it != lib.FnByName.end())
                return it->second.VAddress;
            break;
        }
    }
    return 0;
}

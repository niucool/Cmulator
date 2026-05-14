/**
 * Cmulator - Utility functions (C++ port of Core/utils.pas)
 */

#pragma once

#include "types.h"

#include <unicorn/unicorn.h>
#include <cstdint>
#include <string>

class TEmu;

// ── Constants ──────────────────────────────────────────────────

constexpr uint64_t MAX_STRING_LEN = 1256;

// ── Register I/O ───────────────────────────────────────────────

uint32_t reg_read_x32(uc_engine* uc, int reg);
uint64_t reg_read_x64(uc_engine* uc, int reg);
bool reg_write_x32(uc_engine* uc, int reg, uint32_t value);
bool reg_write_x64(uc_engine* uc, int reg, uint64_t value);

// ── Stack operations ───────────────────────────────────────────

bool push_value(uint64_t value);
uint64_t pop_value();

// ── String I/O (emulated memory) ───────────────────────────────

AnsiString ReadStringA(uint64_t addr, uint32_t len = 0);
AnsiString ReadStringW(uint64_t addr, uint32_t len = 0);
uint32_t WriteStringA(uint64_t addr, const AnsiString& str);
uint32_t WriteStringW(uint64_t addr, const AnsiString& str);

// ── Memory I/O ─────────────────────────────────────────────────

bool WriteMem(uint64_t addr, const void* value, uint32_t len);
bool WriteByte(uint64_t addr, uint8_t value);
bool WriteWord(uint64_t addr, uint16_t value);
bool WriteDword(uint64_t addr, uint32_t value);
bool WriteQword(uint64_t addr, uint64_t value);

ByteArray ReadMem(uint64_t addr, uint32_t len);
uint8_t   ReadByte(uint64_t addr);
uint16_t  ReadWord(uint64_t addr);
int32_t   ReadDword(uint64_t addr);
int64_t   ReadQword(uint64_t addr);

// ── Disassembly (Zydis) ────────────────────────────────────────

// returns a ZydisDecodedInstruction; caller includes <Zydis/Zydis.h>
// declaration: DisAsm(code, addr, size)

// ── Dumps ──────────────────────────────────────────────────────

void HexDump(const uint8_t* mem, int len, uint64_t virtualAddr = 0, int cols = 16);
void DumpStack(uint64_t addr = 0, uint32_t size = 16);

// ── Char checks ────────────────────────────────────────────────

bool cmu_isprint(char c);
bool IsStringPrintable(const std::string& str);

// ── Console colors ─────────────────────────────────────────────

enum ConsoleColor : uint8_t {
    LightRed     = 1,
    LightMagenta = 2,
    Yellow       = 3,
    LightCyan    = 4,
    Magenta      = 5,
    LightBlue    = 6,
};

void TextColor(uint8_t color);
void NormVideo();

// ── DLL / Module helpers ───────────────────────────────────────

uint64_t CmuGetModulehandle(const std::string& module);
uint64_t GetProcAddr(uint64_t handle, const std::string& fnName);
UnicodeString GetFullPath(const std::string& name);
UnicodeString GetDllFromApiSet(const std::string& name);
std::string SplitReg(const std::string& str);

// ── String utilities ───────────────────────────────────────────

std::string ExtractFileNameWithoutExt(const std::string& filename);
std::string UTF32CharToUTF8(uint32_t u4c);

#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Note: These need unicorn and zydis headers to be installed/available.
// #include <unicorn/unicorn.h>
// #include <Zydis/Zydis.h>

// Forward declarations for compilation before library linking setup
struct uc_engine;
struct ZydisDecodedInstruction;

typedef std::vector<uint8_t> TArray;

void HexDump(uint8_t* mem, int len, uint64_t VirtualAddr = 0, uint8_t COLS = 16);
void DumpStack(uint64_t Addr, uint32_t Size);

uint32_t reg_read_x32(uc_engine* uc, int reg);
uint64_t reg_read_x64(uc_engine* uc, int reg);

bool reg_write_x64(uc_engine* uc, int reg, uint64_t value);
bool reg_write_x32(uc_engine* uc, int reg, uint32_t value);

ZydisDecodedInstruction DisAsm(void* code, uint64_t Addr, uint32_t Size);

// Emulator JS Wrappers
uint64_t CmuGetModulehandle(const std::string& Module);
uint64_t GetProcAddr(uint64_t Handle, const std::string& FnName);
bool push(uint64_t value);
uint64_t pop();

std::string ReadStringA(uint64_t Addr, uint32_t len = 0);
std::string ReadStringW(uint64_t Addr, uint32_t len = 0);

uint32_t WriteStringA(uint64_t Addr, const std::string& Str);
uint32_t WriteStringW(uint64_t Addr, const std::string& Str);

bool WriteMem(uint64_t Addr, void* value, uint32_t len);
bool WriteByte(uint64_t Addr, uint8_t value);
bool WriteWord(uint64_t Addr, uint16_t value);
bool WriteDword(uint64_t Addr, uint32_t value);
bool WriteQword(uint64_t Addr, uint64_t value);

TArray ReadMem(uint64_t Addr, uint32_t len);
uint8_t ReadByte(uint64_t Addr);
uint16_t ReadWord(uint64_t Addr);
int32_t ReadDword(uint64_t Addr);
int64_t ReadQword(uint64_t Addr);

bool isprint(char AC);

std::string ExtractFileNameWithoutExt(const std::string& AFilename);

std::u16string GetFullPath(const std::string& name);
std::u16string GetDllFromApiSet(const std::string& name);
std::string SplitReg(const std::string& Str);

void TextColor(uint8_t Color);
void NormVideo();

const uint32_t UC_PAGE_SIZE = 0x1000;
const uint32_t EM_IMAGE_BASE = 0x400000;

// Console Colors
const uint8_t LightRed = 1;
const uint8_t LightMagenta = 2;
const uint8_t Yellow = 3;
const uint8_t LightCyan = 4;
const uint8_t Magenta = 5;
const uint8_t LightBlue = 6;

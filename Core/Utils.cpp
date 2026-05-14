#include "Utils.h"
#include "Globals.h"
#include <iostream>
#include <iomanip>
#include <algorithm>

// Stubs for Utils.cpp

void HexDump(uint8_t* mem, int len, uint64_t VirtualAddr, uint8_t COLS) {
    // Ported logic for HexDump
}

void DumpStack(uint64_t Addr, uint32_t Size) {
    // Ported logic for DumpStack
}

uint32_t reg_read_x32(uc_engine* uc, int reg) {
    uint32_t Result = 0;
    // uc_reg_read(uc, reg, &Result);
    return Result;
}

uint64_t reg_read_x64(uc_engine* uc, int reg) {
    uint64_t Result = 0;
    // uc_reg_read(uc, reg, &Result);
    return Result;
}

bool reg_write_x64(uc_engine* uc, int reg, uint64_t value) {
    // uc_err err = uc_reg_write(uc, reg, &value);
    // return err == UC_ERR_OK;
    return true;
}

bool reg_write_x32(uc_engine* uc, int reg, uint32_t value) {
    // uc_err err = uc_reg_write(uc, reg, &value);
    // return err == UC_ERR_OK;
    return true;
}

ZydisDecodedInstruction DisAsm(void* code, uint64_t Addr, uint32_t Size) {
    ZydisDecodedInstruction Result;
    // ZydisDecoderDecodeBuffer(decoder, code, Size, Addr, &Result);
    return Result;
}

uint64_t CmuGetModulehandle(const std::string& Module) {
    return 0; // Stub
}

uint64_t GetProcAddr(uint64_t Handle, const std::string& FnName) {
    return 0; // Stub
}

bool push(uint64_t value) {
    return false; // Stub
}

uint64_t pop() {
    return 0; // Stub
}

std::string ReadStringA(uint64_t Addr, uint32_t len) {
    return ""; // Stub
}

std::string ReadStringW(uint64_t Addr, uint32_t len) {
    return ""; // Stub
}

uint32_t WriteStringA(uint64_t Addr, const std::string& Str) {
    return 0; // Stub
}

uint32_t WriteStringW(uint64_t Addr, const std::string& Str) {
    return 0; // Stub
}

bool WriteMem(uint64_t Addr, void* value, uint32_t len) {
    return false; // Stub
}

bool WriteByte(uint64_t Addr, uint8_t value) { return false; }
bool WriteWord(uint64_t Addr, uint16_t value) { return false; }
bool WriteDword(uint64_t Addr, uint32_t value) { return false; }
bool WriteQword(uint64_t Addr, uint64_t value) { return false; }

TArray ReadMem(uint64_t Addr, uint32_t len) {
    return TArray(len, 0); // Stub
}

uint8_t ReadByte(uint64_t Addr) { return 0; }
uint16_t ReadWord(uint64_t Addr) { return 0; }
int32_t ReadDword(uint64_t Addr) { return 0; }
int64_t ReadQword(uint64_t Addr) { return 0; }

bool isprint(char AC) {
    return (AC >= ' ') && (AC <= '~') && (AC != 0x7F);
}

std::string ExtractFileNameWithoutExt(const std::string& AFilename) {
    size_t lastSlash = AFilename.find_last_of("\\/");
    std::string name = (lastSlash == std::string::npos) ? AFilename : AFilename.substr(lastSlash + 1);
    size_t lastDot = name.find_last_of('.');
    if (lastDot != std::string::npos) {
        return name.substr(0, lastDot);
    }
    return name;
}

std::u16string GetFullPath(const std::string& name) {
    return u""; // Stub
}

std::u16string GetDllFromApiSet(const std::string& name) {
    return u""; // Stub
}

std::string SplitReg(const std::string& Str) {
    return Str; // Stub
}

void TextColor(uint8_t Color) {
    // Console color logic
}

void NormVideo() {
    // Reset console color logic
}

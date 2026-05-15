#pragma once
#include "types.h"
#include "tep_peb.h"
#include "fnhook.h"
#include <unicorn/unicorn.h>

bool InitTEB_PEB(uc_engine* uc, uint64_t fs, uint64_t gs, uint64_t PEB,
                 uint64_t stack_address, uint64_t stack_limit, bool x64);

void BuildPEB(uc_engine* uc, uint64_t PEB, bool x64);
void BuildPEB_Ldr(uc_engine* uc, uint64_t offset, bool x64);

int GetModulesCount_Excluded(const FastHashMap<std::string, TNewDll>& libs);

LdrDataTableEntry32 BuildPEB_Ldr_Entry32(uc_engine* uc, uint64_t offset,
    uint64_t blink, const TNewDll& lib, const std::string& main_path, bool isEnd);
LdrDataTableEntry64 BuildPEB_Ldr_Entry64(uc_engine* uc, uint64_t offset,
    uint64_t blink, const TNewDll& lib, const std::string& main_path, bool isEnd);

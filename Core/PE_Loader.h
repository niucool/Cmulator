/**
 * Cmulator - PE Loader (C++ port of Core/pe_loader.pas)
 */

#pragma once

#include "types.h"
#include "pe_image.h"
#include "fnhook.h"

#include <unicorn/unicorn.h>
#include <string>

class TEmu;

bool load_sys_dll(uc_engine* uc, const std::string& dll);
void HookImports(uc_engine* uc, PEImage& img);
void InitTLS(uc_engine* uc, PEImage& img);
void InitDll(uc_engine* uc, TNewDll& lib);
void Init_dlls();
MemoryStream MapToMemory(PEImage& pe);
int GetModulesCount(const FastHashMap<std::string, TNewDll>& libs);

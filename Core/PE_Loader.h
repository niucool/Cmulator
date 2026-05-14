#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "Struct.h"
#include <unicorn/unicorn.h>

// Forward declarations to represent the PE library
class TPEImage;
class TMemoryStream;

TMemoryStream* MapPE(TPEImage* Img, const std::string& Path);
void HookImports(uc_engine* uc, TPEImage* Img);
void HookImports_Pse(uc_engine* uc, TPEImage* Img, const std::string& FilePath);
bool load_sys_dll(uc_engine* uc, const std::string& Dll);
TMemoryStream* MapToMemory(TPEImage* PE);

void InitTLS(uc_engine* uc, TPEImage* img);
void Init_dlls();

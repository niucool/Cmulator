#include "PE_Loader.h"
#include "Globals.h"
#include "Utils.h"
#include <iostream>

TMemoryStream* MapPE(TPEImage* Img, const std::string& Path) {
    // Stub
    return nullptr;
}

void HookImports(uc_engine* uc, TPEImage* Img) {
    // Stub
}

void HookImports_Pse(uc_engine* uc, TPEImage* Img, const std::string& FilePath) {
    // Stub
}

bool load_sys_dll(uc_engine* uc, const std::string& Dll) {
    // Stub
    return false;
}

TMemoryStream* MapToMemory(TPEImage* PE) {
    // Stub
    return nullptr;
}

void InitTLS(uc_engine* uc, TPEImage* img) {
    // Stub
}

void Init_dlls() {
    // Stub
}

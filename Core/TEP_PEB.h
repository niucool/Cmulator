#pragma once

#include <cstdint>
#include "Struct.h"
#include <unicorn/unicorn.h>

void BuildPEB_Ldr(uc_engine* uc, uint64_t PEB, bool isx64);
void Init_TEB_PEB(uc_engine* uc, bool isx64);

#pragma once
#include "types.h"
#include "fnhook.h"
#include "segments.h"
#include "pe_image.h"
#include <unicorn/unicorn.h>
#include <string>
#include <cstdint>

struct THooks {
    FastHashMap<uint64_t, THookFunction> ByName;
    FastHashMap<uint64_t, THookFunction> ByOrdinal;
    FastHashMap<uint64_t, THookFunction> ByAddr;
};

class TEmu {
public:
    uc_engine* uc        = nullptr;
    uc_context* tContext  = nullptr;
    uc_err     err       = UC_ERR_OK;

    bool    Is_x64  = false;
    bool&   isx64    = Is_x64;
    bool    IsSC    = false;
    bool    Stop_Emu = false;
    bool&   Stop     = Stop_Emu;

    PEImage img;

    uint32_t stack_size  = 0;
    uint64_t stack_base  = 0;
    uint64_t stack_limit = 0;
    uint64_t DLL_BASE_LOAD = EM_IMAGE_BASE * 2;
    uint64_t DLL_NEXT_LOAD = EM_IMAGE_BASE * 2;

    FastHashMap<std::string, TNewDll> Libs;
    FastHashMap<std::string, TApiRed> ApiSetSchema;
    THooks Hooks;

    bool    RunOnDll    = false;
    bool    IsException = false;
    int64_t SEH_Handler = 0;

    TEmu() = default;
    virtual ~TEmu() = default;
};

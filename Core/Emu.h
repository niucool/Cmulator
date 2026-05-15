#pragma once
#include "types.h"
#include "fnhook.h"
#include "segments.h"
#include "PE/pe_image.h"
#include <unicorn/unicorn.h>
#include <string>
#include <cstdint>
#include <stack>

struct THooks {
    FastHashMap<uint64_t, THookFunction> ByName;
    FastHashMap<uint64_t, THookFunction> ByOrdinal;
    FastHashMap<uint64_t, THookFunction> ByAddr;
};

#include <Zydis/Zydis.h>

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
    uint64_t DLL_BASE_LOAD = 0x70000000;
    uint64_t DLL_NEXT_LOAD = 0x70000000;

    ZydisFormatter Formatter;

    FastHashMap<std::string, TNewDll> Libs;
    FastHashMap<std::string, TApiRed> ApiSetSchema;
    THooks Hooks;
    FastHashMap<uint64_t, THookFunction> OnExitList;

    bool    RunOnDll    = false;
    bool    IsException = false;
    int64_t SEH_Handler = 0;
    uint32_t PID        = 0;
    uint8_t  tmpbool    = 0;

    // GDT / segments
    uint64_t gdt_address = 0;
    uc_x86_mmr gdtr{};

    // TEB / PEB addresses
    uint64_t TEB_Address  = 0;
    uint64_t PEB_address  = 0;
    uint64_t fs_address   = 0;
    uint64_t gs_address   = 0;

    // Segment register values
    uint32_t r_cs = 0, r_ss = 0, r_ds = 0, r_es = 0, r_fs = 0, r_gs = 0;

    // Emulation state
    uint64_t Entry      = 0;
    uint64_t LastGoodPC = 0;
    TFlags   Flags;

    // Memory fixup stacks
    std::stack<uint64_t> MemFix;
    std::stack<flush_r>  FlushMem;

    // Methods
    TEmu() = default;
    virtual ~TEmu() = default;

    bool init_segments();
    void SetHooks();
    bool MapPEtoUC();
    void Start();

    bool SaveCPUState();
    bool RestoreCPUState();
    void ResetEFLAGS();
    void* GetGDT(int index);

    // Callbacks & Helpers
    static bool Handle_SEH(uc_engine* uc, uint32_t ExceptionCode);
    static bool HookMemInvalid(uc_engine* uc, uc_mem_type type, uint64_t address, int size, int64_t value, void* user_data);
    static void HookMemX86(uc_engine* uc, uc_mem_type type, uint64_t address, int size, int64_t value, void* user_data);
    static void HookCode(uc_engine* uc, uint64_t address, uint32_t size, void* user_data);
    static void HookIntr(uc_engine* uc, uint32_t intno, void* user_data);
    static void HookSysCall(uc_engine* uc, void* user_data);
    static void HookSysEnter(uc_engine* uc, void* user_data);

    static void FlushMemMapping(uc_engine* uc);
    static bool CallJS(TLibFunction& API, THookFunction& Hook, uint64_t ret);
    static TApiInfo CheckHook(uc_engine* uc, uint64_t PC);
    static void CheckOnExitCallBack(bool IsAPI, uint64_t PC);
    static bool CheckAddrHooks(uint64_t PC);
};

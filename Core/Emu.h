#pragma once
#include "types.h"
#include "FnHook.h"
#include "Segments.h"
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
    // ── Constructor / Destructor ──────────────────────────────────
    TEmu(const std::string& _FilePath, bool _Shellcode = false, bool SCx64 = false);
    virtual ~TEmu();

    // ── Emulation engine ──────────────────────────────────────────
    uc_engine* uc       = nullptr;
    uc_context* tContext = nullptr;
    uc_err      err     = UC_ERR_OK;

    // ── Architecture ──────────────────────────────────────────────
    bool    Is_x64   = false;
    bool&   isx64     = Is_x64;
    bool    isShellCode = false;
    bool    IsSC     = false;
    bool    Stop_Emu = false;
    bool&   Stop      = Stop_Emu;
    uint32_t tmpbool  = 0;
    bool    RunOnDll  = false;
    bool    IsException = false;
    int64_t SEH_Handler = 0;

    // ── PE ────────────────────────────────────────────────────────
    PEImage img;
    std::string FilePath;
    uint64_t ImageBase = 0; // alias for img.ImageBase

    // ── Stack ─────────────────────────────────────────────────────
    uint32_t stack_size  = 0;
    uint64_t stack_base  = 0;
    uint64_t stack_limit = 0;

    // ── DLL loading ───────────────────────────────────────────────
    uint64_t DLL_BASE_LOAD = 0x0000000700000000ULL;
    uint64_t DLL_NEXT_LOAD = 0x0000000700000000ULL;

    // ── Zydis ─────────────────────────────────────────────────────
    ZydisFormatter Formatter;

    // ── State maps ────────────────────────────────────────────────
    FastHashMap<std::string, TNewDll> Libs;
    FastHashMap<std::string, TApiRed>  ApiSetSchema;
    THooks Hooks;
    FastHashMap<uint64_t, THookFunction> OnExitList;

    // ── GDT / segments ────────────────────────────────────────────
    uint64_t gdt_address = 0;
    uc_x86_mmr gdtr{};
    uint64_t TEB_Address = 0, PEB_address = 0;
    uint64_t fs_address = 0, gs_address = 0;
    uint32_t r_cs=0, r_ss=0, r_ds=0, r_es=0, r_fs=0, r_gs=0;

    // ── Emulation state ───────────────────────────────────────────
    uint64_t Entry      = 0;
    uint64_t LastGoodPC = 0;
    uint64_t PID        = 0;
    TFlags   Flags;

    std::stack<uint64_t> MemFix;
    std::stack<flush_r>  FlushMem;

    // ── Methods ───────────────────────────────────────────────────
    bool init_segments();
    void SetHooks();
    bool MapPEtoUC();
    void Start();

    bool SaveCPUState();
    bool RestoreCPUState();
    void ResetEFLAGS();
    void* GetGDT(int index);

    static bool Handle_SEH(uc_engine* uc, uint32_t ExceptionCode);
    static bool HookMemInvalid(uc_engine* uc, uc_mem_type type, uint64_t addr, int size, int64_t val, void* user);
    static void HookMemX86(uc_engine* uc, uc_mem_type type, uint64_t addr, int size, int64_t val, void* user);
    static void HookCode(uc_engine* uc, uint64_t addr, uint32_t size, void* user);
    static void HookIntr(uc_engine* uc, uint32_t intno, void* user_data);
    static void HookSysCall(uc_engine* uc, void* user_data);
    static void HookSysEnter(uc_engine* uc, void* user_data);
    static void FlushMemMapping(uc_engine* uc);
    static bool CallJS(TLibFunction& API, THookFunction& Hook, uint64_t ret);
    static TApiInfo CheckHook(uc_engine* uc, uint64_t PC);
    static void CheckOnExitCallBack(bool IsAPI, uint64_t PC);
    static bool CheckAddrHooks(uint64_t PC);
};

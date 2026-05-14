#pragma once

#include <cstdint>
#include <string>
#include <map>
#include <vector>
#include <stack>

#include "Globals.h"
#include "Struct.h"
// #include "Segments.h"
// #include "Utils.h"
// #include "PE_Loader.h"

// Forward declaration of dependencies not yet fully defined
class TPEImage;
struct uc_engine;
struct uc_context;

namespace Zydis {
    class TZydisFormatter;
}

typedef std::map<std::string, void*> TLibs; // Should map to TNewDll
typedef std::map<DWORD, void*> TOnExit; // Should map to THookFunction

typedef std::map<uint64_t, void*> THookByName;
typedef std::map<uint64_t, void*> THookByOrdinal;
typedef std::map<uint64_t, void*> THookByAddress;

struct THooks {
    THookByName ByName;
    THookByOrdinal ByOrdinal;
    THookByAddress ByAddr;
};

struct flush_r {
    uint64_t address;
    int64_t value;
    uint32_t size;
};

struct TApiRed {
    uint8_t count;
    std::string first;
    std::string last;
    std::string _alias;
};

typedef std::map<std::string, TApiRed> TApiSetSchema;

struct TApiInfo {
    bool ISAPI;
    bool APIHandled;
    bool CalledFromMainExe;
};

struct TFlags {
    uint32_t FLAGS; // eflags
};

typedef uint32_t uc_mode;
typedef int uc_err;
struct PSegmentDescriptor;
struct uc_x86_mmr;
class TMemoryStream;

class TEmu {
private:
    uc_mode CPU_MODE;
    std::string FilePath;
    std::string Shellcode;
    bool Is_x64;
    bool IsSC;
    bool Stop_Emu;
    uint8_t tmpbool;

    uc_err Ferr;

    PSegmentDescriptor* gdt;
    uint64_t gdt_address;
    uint64_t TEB_Address;
    uint64_t PEB_address;
    uint64_t fs_address;
    uint64_t gs_address;
    // uc_x86_mmr gdtr;

    int64_t SP;

    TMemoryStream* PE;
    TMemoryStream* SCode;
    void* MapedPE;

    TLibs FLibs;
    TOnExit OnExitList;

public:
    Zydis::TZydisFormatter* Formatter;

    uint64_t Entry;
    uint64_t LastGoodPC;
    TFlags Flags;
    DWORD r_cs, r_ss, r_ds, r_es, r_fs, r_gs;

    std::stack<uint64_t> MemFix;
    std::stack<flush_r> FlushMem;

    bool RunOnDll;
    bool IsException;
    int64_t SEH_Handler;

    uint64_t DLL_BASE_LOAD;
    uint64_t DLL_NEXT_LOAD;

    uint32_t stack_size;
    uint64_t stack_base;
    uint64_t stack_limit;

    uint32_t PID;

    TPEImage* Img;
    uc_engine* uc;
    uc_context* tContext;
    THooks Hooks;

    TApiSetSchema ApiSetSchema;

    uint64_t getTEB() const { return TEB_Address; }
    void setTEB(uint64_t val) { TEB_Address = val; }

    uint64_t getPEB() const { return PEB_address; }
    void setPEB(uint64_t val) { PEB_address = val; }

    TLibs& getLibs() { return FLibs; }
    void setLibs(const TLibs& val) { FLibs = val; }

    bool getIsx64() const { return Is_x64; }

    bool getIsShellCode() const { return IsSC; }
    void setIsShellCode(bool val) { IsSC = val; }

    bool getStop() const { return Stop_Emu; }
    void setStop(bool val) { Stop_Emu = val; }

    uc_err getErr() const { return Ferr; }
    void setErr(uc_err val) { Ferr = val; }

    void SetHooks();
    bool MapPEtoUC();
    void Start();
    bool SaveCPUState();
    bool RestoreCPUState();
    void ResetEFLAGS();
    bool init_segments();
    void* GetGDT(int index);

    TEmu(const std::string& _FilePath, bool _Shellcode, bool SCx64);
    virtual ~TEmu();
};

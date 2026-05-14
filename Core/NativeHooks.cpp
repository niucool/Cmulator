/**
 * Cmulator - Native API hooks implementation
 */

#include "nativehooks.h"
#include "globals.h"
#include "emu.h"
#include "utils.h"
#include "tep_peb.h"

#include <plog/Log.h>
#include <xxhash.h>

/** Pascal: native NtContinue handler */
static bool NtContinue(uint64_t uc_handle, uint64_t address, uint64_t ret) {
    uc_engine* uc = reinterpret_cast<uc_engine*>(uc_handle);
    (void)address; (void)ret;
    if (!Emulator) return false;

    uint64_t ExceptionRec = pop_value();
    pop_value(); // old ESP
    uint64_t Context = pop_value();

    Context32 ctx;
    Emulator->err = uc_mem_read(uc, Context, &ctx, sizeof(ctx));
    if (Emulator->err != UC_ERR_OK) {
        PLOG_ERROR << "NtContinue: failed to read Context";
        return false;
    }

    uc_reg_write(uc, Emulator->isx64 ? UC_X86_REG_RBP : UC_X86_REG_EBP, &ctx.Ebp);
    uc_reg_write(uc, Emulator->isx64 ? UC_X86_REG_RSP : UC_X86_REG_ESP, &ctx.Esp);
    uc_reg_write(uc, Emulator->isx64 ? UC_X86_REG_RIP : UC_X86_REG_EIP, &ctx.Eip);
    uc_reg_write(uc, Emulator->isx64 ? UC_X86_REG_RDI : UC_X86_REG_EDI, &ctx.Edi);
    uc_reg_write(uc, Emulator->isx64 ? UC_X86_REG_RSI : UC_X86_REG_ESI, &ctx.Esi);
    uc_reg_write(uc, Emulator->isx64 ? UC_X86_REG_RBX : UC_X86_REG_EBX, &ctx.Ebx);
    uc_reg_write(uc, Emulator->isx64 ? UC_X86_REG_RDX : UC_X86_REG_EDX, &ctx.Edx);
    uc_reg_write(uc, Emulator->isx64 ? UC_X86_REG_RCX : UC_X86_REG_ECX, &ctx.Ecx);
    uc_reg_write(uc, Emulator->isx64 ? UC_X86_REG_RAX : UC_X86_REG_EAX, &ctx.Eax);

    PLOG_DEBUG << "NtContinue: restored context, EIP=0x" << std::hex << ctx.Eip;
    return true;
}

void InstallNativeHooks() {
    uint64_t hash = XXH64("ntdll.NtContinue", 16, 0);
    Emulator->Hooks.ByName[hash] = THookFunction(
        "ntdll", "NtContinue", 0, false,
        NtContinue, JS_UNDEFINED, JS_UNDEFINED, JS_UNDEFINED);
}

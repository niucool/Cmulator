/**
 * Cmulator - Native API hooks implementation (C++ port of Core/nativehooks.pas)
 */

#include "NativeHooks.h"
#include "Globals.h"
#include "Emu.h"
#include "Utils.h"
#include "TEP_PEB.h"

#include <plog/Log.h>
#include <xxhash.h>

static bool NtContinue(uint64_t uc_handle, uint64_t address, uint64_t ret) {
    uc_engine* uc = reinterpret_cast<uc_engine*>(uc_handle);
    (void)address; (void)ret;
    if (!Emulator) return false;

    pop_value();               // ExceptionRec (Pascal reads it for validation)
    pop_value();               // old ESP
    uint64_t Context = pop_value();

    Context32 ctx{};
    Emulator->err = uc_mem_read(uc, Context, &ctx, sizeof(ctx));
    if (Emulator->err != UC_ERR_OK) {
        PLOG_ERROR << "NtContinue: failed to read ContextRecord";
        return false;
    }

    reg_write_x32(uc, (int)UC_X86_REG_EDI, ctx.Edi);
    reg_write_x32(uc, (int)UC_X86_REG_ESI, ctx.Esi);
    reg_write_x32(uc, (int)UC_X86_REG_EBX, ctx.Ebx);
    reg_write_x32(uc, (int)UC_X86_REG_EDX, ctx.Edx);
    reg_write_x32(uc, (int)UC_X86_REG_ECX, ctx.Ecx);
    reg_write_x32(uc, (int)UC_X86_REG_EAX, ctx.Eax);
    reg_write_x32(uc, (int)UC_X86_REG_EBP, ctx.Ebp);
    reg_write_x32(uc, (int)UC_X86_REG_EIP, ctx.Eip);
    reg_write_x32(uc, (int)UC_X86_REG_ESP, ctx.Esp);

    Emulator->Flags.FLAGS = ctx.EFlags;
    reg_write_x32(uc, (int)UC_X86_REG_EFLAGS, (uint32_t)Emulator->Flags.FLAGS);

    if (VerboseExcp) {
        PLOG_INFO << "ZwContinue -> Context = 0x" << std::hex << Context;
    }

    return true;
}

void InstallNativeHooks() {
    uint64_t hash = XXH64("ntdll.NtContinue", 16, 0);
    Emulator->Hooks.ByName[hash] = THookFunction(
        "ntdll", "NtContinue", 0, false,
        NtContinue, JS_UNDEFINED, JS_UNDEFINED, JS_UNDEFINED);
}

#include "emu.h"
#include "globals.h"
#include "utils.h"
#include "fnhook.h"
#include "js_engine.h"
#include "TEP_PEB.h"
#include "Utils.h"

#include <xxhash.h>
#include <quickjs-libc.h>

#include <plog/Log.h>
#include <iomanip>
#include <iostream>
#include <filesystem>

static uint64_t lastExceptionHandler = 0;
extern uint64_t Ident;


// -- TEmu Hooks Implementation --

bool TEmu::Handle_SEH(uc_engine* uc, uint32_t ExceptionCode) {
    if (!Emulator) return false;
    uint64_t ZwContinue = 0, PC = 0, Zer0 = 0, Old_ESP = 0, New_ESP = 0;
    int64_t SEH = 0, SEH_Handler = 0;

    Emulator->err = uc_reg_read(uc, Emulator->Is_x64 ? UC_X86_REG_RSP : UC_X86_REG_ESP, &Old_ESP);
    Emulator->err = uc_reg_read(uc, Emulator->Is_x64 ? UC_X86_REG_RIP : UC_X86_REG_EIP, &PC);
    
    ZwContinue = GetProcAddr(CmuGetModulehandle("ntdll.dll"), "ZwContinue");

    SEH = ReadDword(Emulator->TEB_Address);
    if (Emulator->err != UC_ERR_OK) return false;

    if (SEH == 0 || SEH == 0xFFFFFFFF) {
        PLOG_ERROR << "SEH Is 0xFFFFFFFF or 0";
        return false;
    }

    SEH_Handler = ReadDword(SEH + 4);
    if (Emulator->err != UC_ERR_OK) return false;

    if (SEH_Handler == 0 || SEH_Handler == 0xFFFFFFFF) return false;

    Emulator->SEH_Handler = SEH_Handler;

    if (SEH_Handler != lastExceptionHandler) {
        lastExceptionHandler = SEH_Handler;
    }

    if (VerboseExcp) {
        PLOG_WARNING << "0x" << std::hex << PC << " Exception caught SEH 0x" << SEH 
                     << " - Handler 0x" << SEH_Handler;
    }

    // Space for the "EXCEPTION_POINTERS"
    for (uint32_t i = 0; i < 376; ++i) {
        push_value(0);
    }

    Emulator->err = uc_reg_read(uc, Emulator->Is_x64 ? UC_X86_REG_RSP : UC_X86_REG_ESP, &New_ESP);

    ExceptionRecord32 ExceptionRecord{};
    ExceptionRecord.ExceptionCode = ExceptionCode;
    ExceptionRecord.ExceptionAddress = static_cast<uint32_t>(PC);

    Context32 ContextRecord{};
    ContextRecord.ContextFlags = 0x1007F;
    ContextRecord.SegGs = Emulator->r_gs;
    ContextRecord.SegFs = Emulator->r_fs;
    ContextRecord.SegEs = Emulator->r_es;
    ContextRecord.SegDs = Emulator->r_ds;

    ContextRecord.Edi = static_cast<uint32_t>(reg_read_x64(uc, UC_X86_REG_EDI));
    ContextRecord.Esi = static_cast<uint32_t>(reg_read_x64(uc, UC_X86_REG_ESI));
    ContextRecord.Ebx = static_cast<uint32_t>(reg_read_x64(uc, UC_X86_REG_EBX));
    ContextRecord.Edx = static_cast<uint32_t>(reg_read_x64(uc, UC_X86_REG_EDX));
    ContextRecord.Ecx = static_cast<uint32_t>(reg_read_x64(uc, UC_X86_REG_ECX));
    ContextRecord.Eax = static_cast<uint32_t>(reg_read_x64(uc, UC_X86_REG_EAX));
    ContextRecord.Ebp = static_cast<uint32_t>(reg_read_x64(uc, UC_X86_REG_EBP));
    ContextRecord.Eip = static_cast<uint32_t>(Emulator->LastGoodPC);
    ContextRecord.SegCs = Emulator->r_cs;
    ContextRecord.EFlags = static_cast<uint32_t>(reg_read_x64(uc, UC_X86_REG_EFLAGS));
    ContextRecord.Esp = static_cast<uint32_t>(Old_ESP);
    ContextRecord.SegSs = Emulator->r_ss;

    uint64_t ExceptionRecord_Addr = New_ESP + 32;
    uint64_t ContextRecord_Addr = ExceptionRecord_Addr + sizeof(ExceptionRecord32);

    Emulator->err = uc_mem_write(uc, ExceptionRecord_Addr, &ExceptionRecord, sizeof(ExceptionRecord));
    Emulator->err = uc_mem_write(uc, ContextRecord_Addr, &ContextRecord, sizeof(ContextRecord));

    push_value(ContextRecord_Addr);
    push_value(SEH);
    push_value(ExceptionRecord_Addr);
    push_value(ZwContinue);

    uc_reg_write(uc, Emulator->Is_x64 ? UC_X86_REG_RAX : UC_X86_REG_EAX, &Zer0);
    uc_reg_write(uc, Emulator->Is_x64 ? UC_X86_REG_RBX : UC_X86_REG_EBX, &Zer0);
    uc_reg_write(uc, Emulator->Is_x64 ? UC_X86_REG_RSI : UC_X86_REG_ESI, &Zer0);
    uc_reg_write(uc, Emulator->Is_x64 ? UC_X86_REG_RDI : UC_X86_REG_EDI, &Zer0);
    uc_reg_write(uc, Emulator->Is_x64 ? UC_X86_REG_RCX : UC_X86_REG_ECX, &SEH_Handler);

    Emulator->IsException = true;
    return true;
}

bool TEmu::HookMemInvalid(uc_engine* uc, uc_mem_type type, uint64_t address, int size, int64_t value, void* user_data) {
    if (Emulator && Emulator->Stop) return false;

    switch (type) {
        case UC_MEM_WRITE_UNMAPPED:
            if (!Emulator->RunOnDll) {
                if (VerboseExcp) {
                    PLOG_WARNING << ">>> EXCEPTION_ACCESS_VIOLATION WRITE_UNMAPPED at 0x" << std::hex << address 
                                 << ", data size = " << std::dec << size << ", value = 0x" << std::hex << value;
                }
                if (Handle_SEH(uc, 0xC0000005)) { // EXCEPTION_ACCESS_VIOLATION
                    Emulator->err = uc_mem_map(uc, address & ~0xFFFULL, UC_PAGE_SIZE, UC_PROT_ALL);
                    Emulator->MemFix.push(address & ~0xFFFULL);
                    return true;
                }
            }
            break;
        case UC_MEM_READ_UNMAPPED:
            if (!Emulator->RunOnDll) {
                if (VerboseExcp) {
                    PLOG_WARNING << "EXCEPTION_ACCESS_VIOLATION READ_UNMAPPED : addr 0x" << std::hex << address 
                                 << ", data size = " << std::dec << size << ", value = 0x" << std::hex << value;
                }
                if (Handle_SEH(uc, 0xC0000005)) {
                    Emulator->err = uc_mem_map(uc, address & ~0xFFFULL, UC_PAGE_SIZE, UC_PROT_ALL);
                    Emulator->MemFix.push(address & ~0xFFFULL);
                    return true;
                }
            }
            break;
        case UC_MEM_FETCH_UNMAPPED:
            if (!Emulator->RunOnDll) {
                PLOG_ERROR << ">>> UC_ERR_FETCH_UNMAPPED : addr 0x" << std::hex << address;
            }
            break;
        default:
            if (!Emulator->RunOnDll) {
                PLOG_ERROR << ">>> Error : addr 0x" << std::hex << address << " - Type " << type;
            }
            return false;
    }
    return false;
}

void TEmu::HookMemX86(uc_engine* uc, uc_mem_type type, uint64_t address, int size, int64_t value, void* user_data) {
    if (Emulator && Emulator->Stop) return;

    if (type == UC_MEM_WRITE) {
        if (!Emulator->RunOnDll && address > Emulator->img.ImageBase) {
            flush_r flush;
            flush.address = address;
            flush.value = value;
            flush.size = size;
            Emulator->FlushMem.push(flush);
        }
    }
}

void TEmu::FlushMemMapping(uc_engine* uc) {
    if (!Emulator) return;
    while (!Emulator->FlushMem.empty()) {
        auto flush = Emulator->FlushMem.top();
        Emulator->FlushMem.pop();
        uc_mem_write(uc, flush.address, &flush.value, flush.size);
    }
}

bool TEmu::CallJS(TLibFunction& API, THookFunction& Hook, uint64_t ret) {
    if (!ctx) return false;

    bool result = false;
    if (JS_IsObject(Hook.JSAPI.JSHook) && JS_IsFunction(ctx, Hook.JSAPI.OnCallBack)) {
        JSValue jsAPI = JS_NewObject(ctx);

        bool isEx = (API.FuncName.find("Ex") == API.FuncName.length() - 2) ||
                    (API.FuncName.find("ExA") == API.FuncName.length() - 3) ||
                    (API.FuncName.find("ExW") == API.FuncName.length() - 3);

        JS_SetPropertyStr(ctx, jsAPI, "IsEx", JS_NewBool(ctx, isEx));
        JS_SetPropertyStr(ctx, jsAPI, "IsWapi", JS_NewBool(ctx, API.FuncName.length() > 0 && API.FuncName.back() == 'W'));
        JS_SetPropertyStr(ctx, jsAPI, "IsFW", JS_NewBool(ctx, API.IsForwarder));
        JS_SetPropertyStr(ctx, jsAPI, "LibName", JS_NewString(ctx, API.LibName.c_str()));
        JS_SetPropertyStr(ctx, jsAPI, "name", JS_NewString(ctx, API.FuncName.c_str()));
        JS_SetPropertyStr(ctx, jsAPI, "FWName", JS_NewString(ctx, API.FWName.c_str()));
        JS_SetPropertyStr(ctx, jsAPI, "Address", JS_NewInt64(ctx, API.VAddress));
        JS_SetPropertyStr(ctx, jsAPI, "Ordinal", JS_NewInt32(ctx, API.ordinal));

        JSValueConst args[2] = { jsAPI, JS_NewInt64(ctx, ret) };
        JSValue aResult = JS_Call(ctx, Hook.JSAPI.OnCallBack, Hook.JSAPI.JSHook, 2, args);

        if (JS_IsException(aResult)) {
            js_std_dump_error(ctx);
            exit(-1);
        }

        if (JS_IsBool(aResult)) {
            result = JS_ToBool(ctx, aResult);
            if (VerboseEx && !Emulator->RunOnDll) {
                PLOG_INFO << "JS Return : " << result;
            }
        }
        
        JS_FreeValue(ctx, args[1]);
        JS_FreeValue(ctx, jsAPI);
        JS_FreeValue(ctx, aResult);
    } else {
        JSValue jsErr = JS_GetException(ctx);
        if (JS_IsException(jsErr)) {
            js_std_dump_error(ctx);
        }
        JS_FreeValue(ctx, jsErr);
    }
    return result;
}

TApiInfo TEmu::CheckHook(uc_engine* uc, uint64_t PC) {
    TApiInfo result{};
    result.ISAPI = false;
    result.APIHandled = false;
    result.CalledFromMainExe = false;
    
    if (!Emulator) return result;

    for (auto& pair : Emulator->Libs) {
        TNewDll& lib = pair.second;
        if (PC > lib.BaseAddress && PC < (lib.BaseAddress + lib.ImageSize)) {
            if (lib.Dllname.length() < 3) continue;

            auto it = lib.FnByAddr.find(PC);
            if (it != lib.FnByAddr.end()) {
                TLibFunction& API = it->second;
                
                if (!Emulator->RunOnDll) Ident -= 3;
                
                uint64_t ret = 0;
                Emulator->err = uc_reg_read(uc, Emulator->Is_x64 ? UC_X86_REG_RSP : UC_X86_REG_ESP, &ret);
                Emulator->err = uc_mem_read(uc, ret, &ret, Emulator->img.ImageWordSize);
                
                THookFunction Hook;
                bool hookFound = false;

                if (API.IsOrdinal) {
                    std::string key = ExtractFileNameWithoutExt(std::filesystem::path(API.LibName.c_str()).filename().string()) + "." + std::to_string(API.ordinal);
                    uint64_t hash = XXH64(key.c_str(), key.size(), 0);
                    auto hit = Emulator->Hooks.ByOrdinal.find(hash);
                    if (hit != Emulator->Hooks.ByOrdinal.end()) { Hook = hit->second; hookFound = true; }
                } else {
                    std::string key = ExtractFileNameWithoutExt(std::filesystem::path(API.LibName.c_str()).filename().string()) + "." + API.FuncName.c_str();
                    uint64_t hash = XXH64(key.c_str(), key.size(), 0);
                    auto hit = Emulator->Hooks.ByName.find(hash);
                    if (hit != Emulator->Hooks.ByName.end()) { Hook = hit->second; hookFound = true; }
                    
                    if (!hookFound && Hook.FuncName.empty() && Hook.ordinal > 0) {
                        auto hit2 = Emulator->Hooks.ByOrdinal.find(API.ordinal);
                        if (hit2 != Emulator->Hooks.ByOrdinal.end()) { Hook = hit2->second; hookFound = true; }
                    }
                }
                
                API.Hits += 1;
                API.Return = ret;
                
                if (hookFound && JS_IsObject(Hook.JSAPI.JSHook) && JS_IsFunction(ctx, Hook.JSAPI.OnExit)) {
                    Hook.API = API;
                    Emulator->OnExitList[ret] = Hook;
                }
                
                result.ISAPI = true;
                if (ret > Emulator->img.ImageBase && ret < (Emulator->img.ImageBase + Emulator->img.SizeOfImage)) {
                    result.CalledFromMainExe = true;
                }
                
                if (Verbose && !Emulator->RunOnDll) {
                    PLOG_INFO << "\n[+] Call to \"" << API.LibName << "." 
                              << (API.FuncName.empty() ? ("#" + std::to_string(API.ordinal)) : API.FuncName) << "\"";
                    PLOG_INFO << "[#] Will Return to : 0x" << std::hex << ret;
                    if (API.IsForwarder) {
                        PLOG_INFO << "[!] Forwarded to : \"" << API.FWName << "\"";
                    }
                }
                
                if (hookFound && JS_IsObject(Hook.JSAPI.JSHook) && JS_IsFunction(ctx, Hook.JSAPI.OnCallBack)) {
                    result.APIHandled = CallJS(API, Hook, ret);
                } else if (hookFound && Hook.NativeCallBack) {
                    result.APIHandled = Hook.NativeCallBack((uint64_t)uc, PC, ret);
                } else {
                    if (VerboseEx || !Emulator->RunOnDll) {
                        PLOG_WARNING << "[x] UnHooked Call to " << API.LibName << "." 
                                     << (API.FuncName.empty() ? ("#" + std::to_string(API.ordinal)) : API.FuncName);
                        PLOG_WARNING << "[#] Will Return to : 0x" << std::hex << ret;
                        PLOG_WARNING << "!!! Stack Pointer May get Corrupted";
                    }
                }
                
                if (!Emulator->Stop && !result.APIHandled) {
                    pop_value(); // pop ret
                    Emulator->err = uc_reg_write(uc, Emulator->Is_x64 ? UC_X86_REG_RIP : UC_X86_REG_EIP, &ret);
                }
                
                break;
            }
        }
    }
    return result;
}

void TEmu::CheckOnExitCallBack(bool IsAPI, uint64_t PC) {
    if (!IsAPI && Emulator) {
        auto it = Emulator->OnExitList.find(PC);
        if (it != Emulator->OnExitList.end()) {
            THookFunction& Hook = it->second;
            if (ctx && JS_IsObject(Hook.JSAPI.JSHook) && Hook.API.Return == PC) {
                if (JS_IsFunction(ctx, Hook.JSAPI.OnExit)) {
                    TLibFunction& API = Hook.API;
                    JSValue jsAPI = JS_NewObject(ctx);
                    
                    bool isEx = (API.FuncName.find("Ex") == API.FuncName.length() - 2) ||
                                (API.FuncName.find("ExA") == API.FuncName.length() - 3) ||
                                (API.FuncName.find("ExW") == API.FuncName.length() - 3);

                    JS_SetPropertyStr(ctx, jsAPI, "IsEx", JS_NewBool(ctx, isEx));
                    JS_SetPropertyStr(ctx, jsAPI, "IsWapi", JS_NewBool(ctx, API.FuncName.length() > 0 && API.FuncName.back() == 'W'));
                    JS_SetPropertyStr(ctx, jsAPI, "IsFW", JS_NewBool(ctx, API.IsForwarder));
                    JS_SetPropertyStr(ctx, jsAPI, "LibName", JS_NewString(ctx, API.LibName.c_str()));
                    JS_SetPropertyStr(ctx, jsAPI, "name", JS_NewString(ctx, API.FuncName.c_str()));
                    JS_SetPropertyStr(ctx, jsAPI, "FWName", JS_NewString(ctx, API.FWName.c_str()));
                    JS_SetPropertyStr(ctx, jsAPI, "Address", JS_NewInt64(ctx, API.VAddress));
                    JS_SetPropertyStr(ctx, jsAPI, "Ordinal", JS_NewInt32(ctx, API.ordinal));
                    
                    JSValueConst args[1] = { jsAPI };
                    JSValue aResult = JS_Call(ctx, Hook.JSAPI.OnExit, Hook.JSAPI.JSHook, 1, args);
                    
                    if (JS_IsException(aResult)) {
                        js_std_dump_error(ctx);
                        exit(-1);
                    }
                    
                    JS_FreeValue(ctx, jsAPI);
                    JS_FreeValue(ctx, aResult);
                }
            }
        }
    }
}

bool TEmu::CheckAddrHooks(uint64_t PC) {
    if (!Emulator || !ctx) return false;
    
    auto it = Emulator->Hooks.ByAddr.find(PC);
    if (it != Emulator->Hooks.ByAddr.end()) {
        THookFunction& Hook = it->second;
        if (JS_IsObject(Hook.JSAPI.JSHook) && JS_IsFunction(ctx, Hook.JSAPI.OnCallBack)) {
            JSValue aResult = JS_Call(ctx, Hook.JSAPI.OnCallBack, Hook.JSAPI.JSHook, 0, nullptr);
            if (JS_IsException(aResult)) {
                js_std_dump_error(ctx);
                exit(-1);
            }
            bool result = false;
            if (JS_IsBool(aResult)) {
                result = JS_ToBool(ctx, aResult);
                if (VerboseEx && !Emulator->RunOnDll) {
                    PLOG_INFO << "Address Hook JS Return : " << result;
                }
            }
            JS_FreeValue(ctx, aResult);
            return result;
        }
    }
    return false;
}

void TEmu::HookIntr(uc_engine* uc, uint32_t intno, void* user_data) {
    if (Emulator && !Emulator->Stop) {
        PLOG_WARNING << "Interrupt 0x" << std::hex << intno << " at 0x"
            << reg_read_x64(uc, UC_X86_REG_RIP);
        if (intno == 3) {
            uint64_t pc = reg_read_x64(uc, UC_X86_REG_RIP);
            pc++;
            uc_reg_write(uc, UC_X86_REG_EIP, &pc);
        }
    }
}

void TEmu::HookSysCall(uc_engine* uc, void* user_data) {
    // Syscall stub
    uint64_t rax = reg_read_x64(uc, UC_X86_REG_EAX);
    PLOG_DEBUG << "Syscall EAX=0x" << std::hex << rax;
    reg_write_x64(uc, UC_X86_REG_RAX, 0);

}

void TEmu::HookSysEnter(uc_engine* uc, void* user_data) {
    // SysEnter stub
    uint64_t eax = reg_read_x64(uc, UC_X86_REG_EAX);
    PLOG_DEBUG << "SysEnter EAX=0x" << std::hex << eax;
    reg_write_x64(uc, UC_X86_REG_EAX, 0);

}


void TEmu::HookCode(uc_engine* uc, uint64_t address, uint32_t size, void* user_data) {
    if (!Emulator) return;
    
    uint64_t PC = 0;
    Emulator->err = uc_reg_read(uc, Emulator->Is_x64 ? UC_X86_REG_RIP : UC_X86_REG_EIP, &PC);
    Emulator->LastGoodPC = PC;

    FlushMemMapping(uc);

    while (!Emulator->MemFix.empty()) {
        uint64_t FixAddr = Emulator->MemFix.top();
        Emulator->MemFix.pop();
        uc_mem_unmap(uc, FixAddr, UC_PAGE_SIZE);
    }

    if (Emulator->IsException) {
        Emulator->err = uc_reg_write(uc, Emulator->Is_x64 ? UC_X86_REG_RIP : UC_X86_REG_EIP, &Emulator->SEH_Handler);
        Emulator->IsException = false;
        return;
    }

    if (Steps_limit != 0 && Steps >= Steps_limit) {
        Emulator->Stop = true;
    }

    if (Emulator->Stop) {
        Emulator->err = uc_emu_stop(uc);
        return;
    }

    if (!Speed) {
        CheckAddrHooks(PC);
    }

    TApiInfo API = CheckHook(uc, PC);
    CheckOnExitCallBack(API.ISAPI, PC);

    bool APIPrintFix = API.ISAPI && !API.APIHandled;

    if (ShowASM && !APIPrintFix && !Emulator->RunOnDll) {
        uint8_t code[15] = {0};
        Emulator->err = uc_mem_read(uc, address, code, 15);
        
        ZydisDecodedInstruction ins;
        ZydisDecodedOperand operands[10];
        
        ZydisDecoder decoder;
        ZydisDecoderInit(&decoder, Emulator->Is_x64 ? ZYDIS_MACHINE_MODE_LONG_64 : ZYDIS_MACHINE_MODE_LEGACY_32, Emulator->Is_x64 ? ZYDIS_STACK_WIDTH_64 : ZYDIS_STACK_WIDTH_32);
        
        if (ZYAN_SUCCESS(ZydisDecoderDecodeInstruction(&decoder, 0, code, 15, &ins))) 
        {
            char buffer[256];
            ZydisFormatterFormatInstruction(&Emulator->Formatter, &ins, operands,
                ins.operand_count_visible, buffer, sizeof(buffer), address, ZYAN_NULL);
                
            std::cout << "0x" << std::hex << address << "| " 
                      << std::string(Ident, ' ') << buffer << std::endl;
            
            if (ins.mnemonic == ZYDIS_MNEMONIC_CALL) Ident += 3;
            if (ins.mnemonic == ZYDIS_MNEMONIC_RET) {
                if (Ident >= 3) Ident -= 3;
                else Ident = 0;
            }
        }
    }

    if (size == 3 && !Emulator->RunOnDll && Emulator->IsSC) {
        uint8_t code[3] = {0};
        uc_mem_read(uc, address, code, 3);
        if (code[0] == 0x31) {
            if (Emulator->tmpbool == 1) {
                PC += size;
                Emulator->err = uc_reg_write(uc, Emulator->Is_x64 ? UC_X86_REG_RIP : UC_X86_REG_EIP, &PC);
                Emulator->tmpbool += 1;
            } else if (Emulator->tmpbool == 0) {
                Emulator->tmpbool += 1;
            }
        }
    }

    if (size == 2) {
        uint8_t code[2] = {0};
        uc_mem_read(uc, address, code, 2);
        // rdtsc
        if (code[0] == 0x0F && code[1] == 0x31) {
            reg_write_x32(uc, UC_X86_REG_EAX, (rand() % 300) + 100);
            reg_write_x32(uc, UC_X86_REG_EDX, (rand() % 300) + 100);
            if (!Emulator->RunOnDll) PLOG_INFO << "rdtsc at 0x" << std::hex << PC;
            PC += size;
            Emulator->err = uc_reg_write(uc, Emulator->Is_x64 ? UC_X86_REG_RIP : UC_X86_REG_EIP, &PC);
        }
        // cpuid
        if (code[0] == 0x0F && code[1] == 0xA2) {
            reg_write_x32(uc, UC_X86_REG_EAX, 0);
            if (!Emulator->RunOnDll) PLOG_INFO << "CPUID at 0x" << std::hex << PC;
        }
    }

    if (!Emulator->RunOnDll && Steps_limit != 0) {
        Steps += 1;
    }
}

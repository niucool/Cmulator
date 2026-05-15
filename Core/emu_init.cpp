#include "emu.h"
#include "globals.h"
#include "segments.h"
#include "utils.h"
#include "process/ethreads.h"
#include "pe_loader.h"
#include "nativehooks.h"
#include "js_engine.h"
#include "fnhook.h"

#include <plog/Log.h>
#include <unicorn/unicorn.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ── GDT helper ─────────────────────────────────────────────────

void* TEmu::GetGDT(int index) {
    return &gdt_address; // placeholder; real GDT is managed by init_segments
}

// ── ResetEFLAGS ────────────────────────────────────────────────

void TEmu::ResetEFLAGS() {
    Flags.FLAGS = 0x202;
    reg_write_x64(uc, UC_X86_REG_EFLAGS, Flags.FLAGS);
}

// ── Save/Restore CPU state ─────────────────────────────────────

bool TEmu::SaveCPUState() {
    if (!tContext) return false;
    return uc_context_save(uc, tContext) == UC_ERR_OK;
}

bool TEmu::RestoreCPUState() {
    if (!tContext) return false;
    return uc_context_restore(uc, tContext) == UC_ERR_OK;
}

// ── SetHooks ───────────────────────────────────────────────────

void TEmu::SetHooks() {
    uc_hook trace3, trace4, trace5, trace6, trace7;

    err = uc_hook_add(uc, &trace3,
        UC_HOOK_MEM_READ_UNMAPPED |
        UC_HOOK_MEM_WRITE_UNMAPPED |
        UC_HOOK_MEM_FETCH_UNMAPPED,
        (void*)(+[](uc_engine* uc, uc_mem_type type, uint64_t address,
           int size, int64_t value, void* user_data) -> bool {
            // HookMemInvalid: minimal stub
            if (Emulator && Emulator->Stop) return false;
            PLOG_DEBUG << "Memory access error at 0x" << std::hex << address;
            // Map the page on demand
            uc_err e = uc_mem_map(uc, address & ~0xFFFULL, 0x1000, UC_PROT_ALL);
            return e == UC_ERR_OK;
        }), nullptr, 1, 0);

    err = uc_hook_add(uc, &trace4, UC_HOOK_CODE,
        (void*)(+[](uc_engine* uc, uint64_t address, uint32_t size, void* user_data) {
            if (!Emulator || Emulator->Stop) { uc_emu_stop(uc); return; }
            if (Steps_limit != 0 && Steps >= Steps_limit) {
                Emulator->Stop = true;
                uc_emu_stop(uc);
                return;
            }
            Steps++;
        }), nullptr, 1, 0);

    err = uc_hook_add(uc, &trace5, UC_HOOK_INTR,
        (void*)(+[](uc_engine* uc, uint32_t intno, void* user_data) {
            if (Emulator && !Emulator->Stop) {
                PLOG_WARNING << "Interrupt 0x" << std::hex << intno << " at 0x"
                             << reg_read_x64(uc, UC_X86_REG_RIP);
                if (intno == 3) {
                    uint64_t pc = reg_read_x64(uc, UC_X86_REG_RIP);
                    pc++;
                    uc_reg_write(uc, UC_X86_REG_EIP, &pc);
                }
            }
        }), nullptr, 1, 0);

    err = uc_hook_add(uc, &trace6, UC_HOOK_INSN,
        (void*)(+[](uc_engine* uc, uint32_t user_data) {
            // Syscall stub
            uint64_t rax = reg_read_x64(uc, UC_X86_REG_EAX);
            PLOG_DEBUG << "Syscall EAX=0x" << std::hex << rax;
            reg_write_x64(uc, UC_X86_REG_RAX, 0);
        }), nullptr, 1, 0, UC_X86_INS_SYSCALL);

    err = uc_hook_add(uc, &trace7, UC_HOOK_INSN,
        (void*)(+[](uc_engine* uc, uint32_t user_data) {
            // SysEnter stub
            uint64_t eax = reg_read_x64(uc, UC_X86_REG_EAX);
            PLOG_DEBUG << "SysEnter EAX=0x" << std::hex << eax;
            reg_write_x64(uc, UC_X86_REG_EAX, 0);
        }), nullptr, 1, 0, UC_X86_INS_SYSENTER);
}

// ── MapPEtoUC ──────────────────────────────────────────────────

bool TEmu::MapPEtoUC() {
    size_t mapSize = (img.SizeOfImage + UC_PAGE_SIZE - 1) & ~(UC_PAGE_SIZE - 1);
    err = uc_mem_map(uc, img.ImageBase, mapSize, UC_PROT_ALL);
    if (err != UC_ERR_OK) {
        PLOG_ERROR << "uc_mem_map PE failed: " << uc_strerror(err);
        return false;
    }

    // Write PE headers and section data in virtual image layout.
    FILE* fp = fopen(img.FileName.c_str(), "rb");
    if (!fp) return false;
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::vector<uint8_t> buf(fsize);
    fread(buf.data(), 1, fsize, fp);
    fclose(fp);

    size_t headerSize = std::min<size_t>(buf.size(), img.SizeOfHeaders);
    if (headerSize == 0) headerSize = std::min<size_t>(buf.size(), UC_PAGE_SIZE);

    err = uc_mem_write(uc, img.ImageBase, buf.data(), headerSize);
    if (err != UC_ERR_OK) {
        PLOG_ERROR << "uc_mem_write PE headers failed: " << uc_strerror(err);
        return false;
    }

    struct SectionWriteCtx {
        uc_engine* uc;
        uint64_t imageBase;
        uc_err err;
    } ctx{uc, img.ImageBase, UC_ERR_OK};

    peparse::IterSec(img._pe, [](void* u, const peparse::VA& secBase,
                       const std::string&, const peparse::image_section_header&,
                       const peparse::bounded_buffer* data) -> int {
        auto* ctx = static_cast<SectionWriteCtx*>(u);
        if (!data || !data->buf || data->bufLen == 0) return 0;

        ctx->err = uc_mem_write(ctx->uc, ctx->imageBase + secBase,
                                data->buf, data->bufLen);
        return ctx->err == UC_ERR_OK ? 0 : 1;
    }, &ctx);

    if (ctx.err != UC_ERR_OK) {
        PLOG_ERROR << "uc_mem_write PE section failed: " << uc_strerror(ctx.err);
        return false;
    }

    printf("[+] PE mapped @ 0x%llx (size=0x%zx)\n",
           (unsigned long long)img.ImageBase, mapSize);
    return true;
}

// ── init_segments ──────────────────────────────────────────────

bool TEmu::init_segments() {
    stack_base  = 0x200000;
    stack_size  = 0x60000;
    stack_limit = stack_base - stack_size;

    gs_address = 0;

    if (Is_x64) {
        fs_address  = 0x7FFFFFC0000ULL;
        gs_address  = 0x7FFFFFE0000ULL;
        TEB_Address = gs_address;
        PEB_address = gs_address - 0x10000;
    } else {
        fs_address  = 0x7FFE0000;
        TEB_Address = fs_address;
        PEB_address = fs_address - 0x10000;
    }
    PEB_address = PEB_address;

    // Set FS/GS MSR (x64 only)
    if (Is_x64) {
        if (!SetGS(uc, gs_address)) {
            PLOG_ERROR << "SetGS failed";
            return false;
        }
    }

    ResetEFLAGS();

    r_cs = CreateSelector(14, S_GDT | S_PRIV_3); // 0x73
    r_ds = CreateSelector(15, S_GDT | S_PRIV_3); // 0x7B
    r_es = CreateSelector(15, S_GDT | S_PRIV_3); // 0x7B
    r_gs = CreateSelector(15, S_GDT | S_PRIV_3); // 0x7B
    r_fs = CreateSelector(16, S_GDT | S_PRIV_3); // 0x83
    r_ss = CreateSelector(17, S_GDT | S_PRIV_0); // 0x88

    gdt_address = 0xC0000000;
    gdtr.base = gdt_address;
    gdtr.limit = 31 * sizeof(TSegmentDescriptor) - 1;

    // Allocate 31 descriptors
    std::vector<TSegmentDescriptor> gdt(31);

    size_t codeIdx = 14, dataIdx = 15, fsIdx = 16, ring0Idx = 17;
    InitDescriptor(&gdt[codeIdx],  0, 0xFFFFF000, true);   // code segment
    InitDescriptor(&gdt[dataIdx],  0, 0xFFFFF000, false);  // data segment
    InitDescriptor(&gdt[fsIdx],    static_cast<uint32_t>(fs_address), 0xFFF, false); // FS segment
    InitDescriptor(&gdt[ring0Idx], 0, 0xFFFFF000, false);  // ring 0 data
    gdt[ring0Idx].dpl = 0;

    // Map large scratch region
    uint64_t scratch = 0x40000000;
    err = uc_mem_map(uc, scratch, 0x30000000, UC_PROT_ALL);
    if (err != UC_ERR_OK) return false;

    // Map stack
    err = uc_mem_map(uc, stack_base, stack_size, UC_PROT_READ | UC_PROT_WRITE);
    if (err != UC_ERR_OK) return false;

    // Map GDT
    err = uc_mem_map(uc, gdt_address, 0x10000, UC_PROT_WRITE | UC_PROT_READ);
    if (err != UC_ERR_OK) return false;

    // Write GDTR
    err = uc_reg_write(uc, UC_X86_REG_GDTR, &gdtr);
    if (err != UC_ERR_OK) return false;

    // Write GDT
    err = uc_mem_write(uc, gdt_address, gdt.data(), 31 * sizeof(TSegmentDescriptor));
    if (err != UC_ERR_OK) return false;

    // Map FS
    err = uc_mem_map(uc, fs_address, 0x4000, UC_PROT_WRITE | UC_PROT_READ);
    if (err != UC_ERR_OK) return false;

    // Set segment registers
    uint64_t sp = ((stack_base + stack_size) - 0x70);
    err = uc_reg_write(uc, UC_X86_REG_ESP, &sp);
    if (err != UC_ERR_OK) return false;
    err = uc_reg_write(uc, UC_X86_REG_SS, &r_ss);
    if (err != UC_ERR_OK) return false;
    err = uc_reg_write(uc, UC_X86_REG_CS, &r_cs);
    if (err != UC_ERR_OK) return false;
    err = uc_reg_write(uc, UC_X86_REG_DS, &r_ds);
    if (err != UC_ERR_OK) return false;
    err = uc_reg_write(uc, UC_X86_REG_ES, &r_es);
    if (err != UC_ERR_OK) return false;
    err = uc_reg_write(uc, UC_X86_REG_FS, &r_fs);
    if (err != UC_ERR_OK) return false;

    // Map GS for x64
    if (Is_x64) {
        err = uc_mem_map(uc, gs_address, 0x4000, UC_PROT_WRITE | UC_PROT_READ);
        if (err != UC_ERR_OK) return false;
    }

    // Map PEB
    err = uc_mem_map(uc, PEB_address, 0x10000, UC_PROT_WRITE | UC_PROT_READ);
    if (err != UC_ERR_OK) return false;

    // Init TEB / PEB
    if (!InitTEB_PEB(uc, fs_address, gs_address, PEB_address,
                     stack_base, static_cast<uint32_t>(stack_limit), Is_x64)) {
        PLOG_ERROR << "InitTEB_PEB failed";
        return false;
    }

    printf("[+] Segments & (TIB - PEB) Init Done\n");
    return true;
}

// ── Start ──────────────────────────────────────────────────────

void TEmu::Start() {
    // Allocate context
    uc_context_alloc(uc, &tContext);
    if (!tContext) {
        printf("[X] Error allocating CPU Context\n");
        return;
    }

    SetHooks();
    printf("[+] Set Hooks\n");

    if (!MapPEtoUC()) return;

    if (!init_segments()) {
        printf("Can't init segments, last Err: %s\n", uc_strerror(err));
        return;
    }

    // Load system DLLs
    if (load_sys_dll(uc, "ntdll.dll") &&
        load_sys_dll(uc, "kernel32.dll") &&
        load_sys_dll(uc, "kernelbase.dll")) {
        load_sys_dll(uc, "ucrtbase.dll");

        // Hook PE imports
        HookImports(uc, img);

        // Native hooks
        InstallNativeHooks();

        // Init JS
        Init_QJS();
        if (!JSAPI.empty())
            LoadScript(JSAPI.c_str());
        InitJSEmu();

        // Init DLLs and TLS
        Init_dlls();
        InitTLS(uc, img);

        printf("\n[>] Run %s\n\n", img.FileName.c_str());

        // Initial stack pointer
        uint64_t sp = ((stack_base + stack_size) - 0x100) & ~0xFFULL;
        err = uc_reg_write(uc, UC_X86_REG_ESP, &sp);
        err = uc_reg_write(uc, UC_X86_REG_EBP, &sp);

        ResetEFLAGS();

        Entry = img.ImageBase + img.EntryPointRVA;

        // Seed EDX/RDX for samples that enter through a call register pattern.
        err = uc_reg_write(uc, Is_x64 ? UC_X86_REG_RDX : UC_X86_REG_EDX, &Entry);

        // Write return address (RtlExitUserThread)
        uint64_t rtlExit = GetProcAddr(CmuGetModulehandle("ntdll.dll"), "RtlExitUserThread");
        WriteDword(sp, static_cast<uint32_t>(rtlExit));

        uint64_t rip = 0;
        err = uc_reg_read(uc, Is_x64 ? UC_X86_REG_RIP : UC_X86_REG_EIP, &rip);
        if (rip != 0) Entry = rip;

        uint64_t startTime = 0, endTime = 0;
#ifdef _WIN32
        startTime = GetTickCount64();
#else
        startTime = 0;
#endif

        err = uc_emu_start(uc, Entry, img.ImageBase + img.SizeOfImage, 0, 0);

#ifdef _WIN32
        endTime = GetTickCount64();
#endif

        printf("\nExecuted in %lld ms\n", (long long)(endTime - startTime));

        uint64_t pc = reg_read_x64(uc, Is_x64 ? UC_X86_REG_RIP : UC_X86_REG_EIP);
        printf("Last Known Good IP: 0x%llx\n", (unsigned long long)pc);
        printf("Cmulator Stop >> last Error: %s\n", uc_strerror(err));
    }
}

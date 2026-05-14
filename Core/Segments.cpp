/**
 * Cmulator - Segments / GDT / MSR implementation (C++ port of Core/segments.pas)
 */

#include "segments.h"

#include <cstdio>
#include <cinttypes>

// ── Segment selector creation ─────────────────────────────────────

uint64_t CreateSelector(uint32_t idx, uint32_t flags) {
    return (static_cast<uint64_t>(idx) << 3) | flags;
}

// ── Descriptor initialization ─────────────────────────────────────

void InitDescriptor(TSegmentDescriptor* desc, uint32_t base, uint32_t limit, bool is_code) {
    if (!desc) return;

    desc->limit0       = limit & 0xFFFF;
    desc->base0        = base & 0xFFFF;
    desc->base1        = (base >> 16) & 0xFF;
    desc->type         = is_code ? 0xA : 0x2;
    desc->system       = 1;
    desc->dpl          = 3;
    desc->present      = 1;
    desc->limit1       = (limit >> 16) & 0xF;
    desc->avail        = 0;
    desc->is_64_code   = is_code ? 1 : 0;
    desc->db           = is_code ? 0 : 1;
    desc->granularity  = 1;
    desc->base2        = (base >> 24) & 0xFF;
}

void InitGDT(TSegmentDescriptor* desc, uint32_t base, uint32_t limit,
             uint32_t access, uint32_t flags) {
    if (!desc) return;

    desc->limit0       = limit & 0xFFFF;
    desc->base0        = base & 0xFFFF;
    desc->base1        = (base >> 16) & 0xFF;
    desc->type         = access & 0xF;
    desc->system       = (access >> 4) & 1;
    desc->dpl          = (access >> 5) & 3;
    desc->present      = (access >> 7) & 1;
    desc->limit1       = (limit >> 16) & 0xF;
    desc->avail        = flags & 1;
    desc->is_64_code   = (flags >> 1) & 1;
    desc->db           = (flags >> 2) & 1;
    desc->granularity  = (flags >> 3) & 1;
    desc->base2        = (base >> 24) & 0xFF;
}

// ── MSR Helper (internal) ─────────────────────────────────────────

static bool setMSR(uc_engine* uc, uint64_t msr, uint64_t value, uint64_t scratch_addr) {
    if (!uc) return false;

    // Save registers
    uint64_t o_rax, o_rdx, o_rcx, o_rip;
    uc_reg_read(uc, UC_X86_REG_RAX, &o_rax);
    uc_reg_read(uc, UC_X86_REG_RDX, &o_rdx);
    uc_reg_read(uc, UC_X86_REG_RCX, &o_rcx);
    uc_reg_read(uc, UC_X86_REG_RIP, &o_rip);

    // wrmsr: 0F 30
    const uint8_t code[] = { 0x0F, 0x30 };

    uc_err err = uc_mem_write(uc, scratch_addr, code, sizeof(code));
    if (err != UC_ERR_OK) return false;

    uint64_t rax_val = value & 0xFFFFFFFF;
    uint64_t rdx_val = (value >> 32) & 0xFFFFFFFF;
    uint64_t rcx_val = msr & 0xFFFFFFFF;

    uc_reg_write(uc, UC_X86_REG_RAX, &rax_val);
    uc_reg_write(uc, UC_X86_REG_RDX, &rdx_val);
    uc_reg_write(uc, UC_X86_REG_RCX, &rcx_val);

    err = uc_emu_start(uc, scratch_addr, scratch_addr + sizeof(code), 0, 1);

    // Restore registers
    uc_reg_write(uc, UC_X86_REG_RAX, &o_rax);
    uc_reg_write(uc, UC_X86_REG_RDX, &o_rdx);
    uc_reg_write(uc, UC_X86_REG_RCX, &o_rcx);
    uc_reg_write(uc, UC_X86_REG_RIP, &o_rip);

    return err == UC_ERR_OK;
}

static bool getMSR(uc_engine* uc, uint64_t msr, uint64_t& value, uint64_t scratch_addr) {
    if (!uc) return false;

    value = 0;

    // Save registers
    uint64_t o_rax, o_rdx, o_rcx, o_rip;
    uc_reg_read(uc, UC_X86_REG_RAX, &o_rax);
    uc_reg_read(uc, UC_X86_REG_RDX, &o_rdx);
    uc_reg_read(uc, UC_X86_REG_RCX, &o_rcx);
    uc_reg_read(uc, UC_X86_REG_RIP, &o_rip);

    // rdmsr: 0F 32
    const uint8_t code[] = { 0x0F, 0x32 };

    uc_err err = uc_mem_write(uc, scratch_addr, code, sizeof(code));
    if (err != UC_ERR_OK) return false;

    uint64_t rcx_val = msr & 0xFFFFFFFF;
    uc_reg_write(uc, UC_X86_REG_RCX, &rcx_val);

    err = uc_emu_start(uc, scratch_addr, scratch_addr + sizeof(code), 0, 1);
    if (err == UC_ERR_OK) {
        uint64_t rax_val, rdx_val;
        uc_reg_read(uc, UC_X86_REG_RAX, &rax_val);
        uc_reg_read(uc, UC_X86_REG_RDX, &rdx_val);
        value = (rdx_val << 32) | (rax_val & 0xFFFFFFFF);
    }

    // Restore registers
    uc_reg_write(uc, UC_X86_REG_RAX, &o_rax);
    uc_reg_write(uc, UC_X86_REG_RDX, &o_rdx);
    uc_reg_write(uc, UC_X86_REG_RCX, &o_rcx);
    uc_reg_write(uc, UC_X86_REG_RIP, &o_rip);

    return err == UC_ERR_OK;
}

// ── Public FS/GS helpers ─────────────────────────────────────────

static constexpr uint64_t SCRATCH_ADDR = 0x80000;

bool SetFS(uc_engine* uc, uint64_t addr) {
    return setMSR(uc, FSMSR, addr, SCRATCH_ADDR);
}

bool GetFS(uc_engine* uc, uint64_t& addr) {
    return getMSR(uc, FSMSR, addr, SCRATCH_ADDR);
}

bool SetGS(uc_engine* uc, uint64_t addr) {
    return setMSR(uc, GSMSR, addr, SCRATCH_ADDR);
}

bool GetGS(uc_engine* uc, uint64_t& addr) {
    return getMSR(uc, GSMSR, addr, SCRATCH_ADDR);
}

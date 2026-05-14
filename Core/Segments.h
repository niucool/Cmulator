/**
 * Cmulator - Segments / GDT / MSR (C++ port of Core/segments.pas)
 *
 * TFlags           — x86 EFLAGS bitfield (union with uint64_t)
 * TSegmentDescriptor — GDT/LDT descriptor (union with uint64_t)
 * GDT constants, segment selector creation, FS/GS MSR helpers
 */

#pragma once

#include "types.h"
#include <unicorn/unicorn.h>

// ── TFlags (x86 EFLAGS) ──────────────────────────────────────────

struct TFlags {
    union {
        struct {
            uint8_t  CF       : 1;   // Carry flag
            uint8_t  Reserved1 : 1;  // Always 1 in EFLAGS
            uint8_t  PF       : 1;   // Parity flag
            uint8_t  Reserved2 : 1;
            uint8_t  AF       : 1;   // Adjust flag
            uint8_t  Reserved3 : 1;
            uint8_t  ZF       : 1;   // Zero flag
            uint8_t  SF       : 1;   // Sign flag
            uint8_t  TF       : 1;   // Trap flag
            uint8_t  IF_val    : 1;  // Interrupt enable
            uint8_t  DF       : 1;   // Direction flag
            uint8_t  OF_val    : 1;  // Overflow flag
            uint8_t  IOPL     : 2;   // I/O privilege level
            uint8_t  NT       : 1;   // Nested task flag
            uint8_t  Reserved4 : 1;
            uint8_t  RF       : 1;   // Resume flag
            uint8_t  VM       : 1;   // Virtual 8086 mode
            uint8_t  AC       : 1;   // Alignment check
            uint8_t  VIF      : 1;   // Virtual interrupt flag
            uint8_t  VIP      : 1;   // Virtual interrupt pending
            uint8_t  ID       : 1;   // CPUID available
            uint8_t  ID2      : 1;
            uint8_t  VAD      : 2;   // VAD flag
            uint32_t Reserved5 : 32; // RFLAGS reserved
        };
        uint64_t FLAGS = 0;
    };

    TFlags() : FLAGS(0) {}
};

// ── TSegmentDescriptor ────────────────────────────────────────────

struct TSegmentDescriptor {
    union {
        struct {
            uint16_t limit0      : 16;
            uint16_t base0       : 16;
            uint8_t  base1       : 8;
            uint8_t  type        : 4;
            uint8_t  system      : 1;   // S flag
            uint8_t  dpl         : 2;
            uint8_t  present     : 1;   // P flag
            uint8_t  limit1      : 4;
            uint8_t  avail       : 1;
            uint8_t  is_64_code  : 1;   // L flag
            uint8_t  db          : 1;   // DB flag
            uint8_t  granularity : 1;   // G flag
            uint8_t  base2       : 8;
        };
        uint64_t desc = 0;
    };

    TSegmentDescriptor() : desc(0) {}
};

// ── Segment flag constants ────────────────────────────────────────

constexpr uint8_t F_GRANULARITY  = 0x8;
constexpr uint8_t F_PROT_32      = 0x4;
constexpr uint8_t F_LONG         = 0x2;
constexpr uint8_t F_AVAILABLE    = 0x1;

constexpr uint8_t A_PRESENT      = 0x80;
constexpr uint8_t A_PRIV_3       = 0x60;
constexpr uint8_t A_PRIV_2       = 0x40;
constexpr uint8_t A_PRIV_1       = 0x20;
constexpr uint8_t A_PRIV_0       = 0x00;
constexpr uint8_t A_CODE         = 0x10;
constexpr uint8_t A_DATA         = 0x10;
constexpr uint8_t A_TSS          = 0x00;
constexpr uint8_t A_GATE         = 0x00;
constexpr uint8_t A_EXEC         = 0x08;
constexpr uint8_t A_DATA_WRITABLE = 0x02;
constexpr uint8_t A_CODE_READABLE = 0x02;
constexpr uint8_t A_DIR_CON_BIT  = 0x04;

constexpr uint8_t S_GDT          = 0x00;
constexpr uint8_t S_LDT          = 0x04;
constexpr uint8_t S_PRIV_3       = 0x03;
constexpr uint8_t S_PRIV_2       = 0x02;
constexpr uint8_t S_PRIV_1       = 0x01;
constexpr uint8_t S_PRIV_0       = 0x00;

constexpr uint64_t FSMSR = 0xC0000100;
constexpr uint64_t GSMSR = 0xC0000101;

// ── Segment functions ─────────────────────────────────────────────

/** Pascal: function CreateSelector(idx, flags : UInt32): UInt64; */
uint64_t CreateSelector(uint32_t idx, uint32_t flags);

/** Pascal: procedure Init_Descriptor(desc : PSegmentDescriptor; base, limit, is_code) */
void InitDescriptor(TSegmentDescriptor* desc, uint32_t base, uint32_t limit, bool is_code);

/** Pascal: procedure Init_GDT(desc : PSegmentDescriptor; base, limit, access, flags) */
void InitGDT(TSegmentDescriptor* desc, uint32_t base, uint32_t limit,
             uint32_t access, uint32_t flags);

// FS/GS MSR manipulation (x64 only)

/** Pascal: function SetFS(uc : uc_engine; addr : UInt64): boolean; */
bool SetFS(uc_engine* uc, uint64_t addr);

/** Pascal: function GetFS(uc : uc_engine; var addr : UInt64): boolean; */
bool GetFS(uc_engine* uc, uint64_t& addr);

/** Pascal: function SetGS(uc : uc_engine; addr : UInt64): boolean; */
bool SetGS(uc_engine* uc, uint64_t addr);

/** Pascal: function GetGS(uc : uc_engine; var addr : UInt64): boolean; */
bool GetGS(uc_engine* uc, uint64_t& addr);

/**
 * Cmulator - TEB / PEB structures (C++ port of Core/tep_peb.pas)
 *
 * Only the structures actively used by Cmulator's emulation core are ported.
 * Full TEB (TTIB_32/64) is ~2KB and mostly reserved/unused in emulation.
 */

#pragma once

#include "types.h"

#ifndef _WIN32
constexpr uint32_t EXCEPTION_ACCESS_VIOLATION      = 0xC0000005;
constexpr uint32_t EXCEPTION_BREAKPOINT             = 0x80000003;
constexpr uint32_t EXCEPTION_DATATYPE_MISALIGNMENT  = 0x80000002;
constexpr uint32_t EXCEPTION_SINGLE_STEP            = 0x80000004;
constexpr uint32_t EXCEPTION_ARRAY_BOUNDS_EXCEEDED  = 0xC000008C;
constexpr uint32_t EXCEPTION_FLT_DENORMAL_OPERAND   = 0xC000008D;
constexpr uint32_t EXCEPTION_FLT_DIVIDE_BY_ZERO     = 0xC000008E;
constexpr uint32_t EXCEPTION_FLT_INEXACT_RESULT     = 0xC000008F;
constexpr uint32_t EXCEPTION_FLT_INVALID_OPERATION  = 0xC0000090;
constexpr uint32_t EXCEPTION_FLT_OVERFLOW           = 0xC0000091;
constexpr uint32_t EXCEPTION_FLT_STACK_CHECK        = 0xC0000092;
constexpr uint32_t EXCEPTION_FLT_UNDERFLOW          = 0xC0000093;
constexpr uint32_t EXCEPTION_INT_DIVIDE_BY_ZERO     = 0xC0000094;
constexpr uint32_t EXCEPTION_INT_OVERFLOW           = 0xC0000095;
constexpr uint32_t EXCEPTION_INVALID_HANDLE         = 0xC0000008;
constexpr uint32_t EXCEPTION_PRIV_INSTRUCTION       = 0xC0000096;
constexpr uint32_t EXCEPTION_NONCONTINUABLE_EXCEPTION = 0xC0000025;
constexpr uint32_t EXCEPTION_NONCONTINUABLE         = 0x00000001;
constexpr uint32_t EXCEPTION_STACK_OVERFLOW         = 0xC00000FD;
constexpr uint32_t EXCEPTION_INVALID_DISPOSITION    = 0xC0000026;
constexpr uint32_t EXCEPTION_IN_PAGE_ERROR          = 0xC0000006;
constexpr uint32_t EXCEPTION_ILLEGAL_INSTRUCTION    = 0xC000001D;
constexpr uint32_t EXCEPTION_POSSIBLE_DEADLOCK      = 0xC0000194;
#endif

#ifndef EXCEPTION_MAXIMUM_PARAMETERS
constexpr int EXCEPTION_MAXIMUM_PARAMETERS = 15;
#endif

// ── EXCEPTION_RECORD_32 ───────────────────────────────────────────

struct ExceptionRecord32 {
    DWORD ExceptionCode    = 0;
    DWORD ExceptionFlags   = 0;
    DWORD ExceptionRecord  = 0;  // Pointer to next EXCEPTION_RECORD_32
    DWORD ExceptionAddress = 0;
    DWORD NumberParameters = 0;
    DWORD ExceptionInformation[EXCEPTION_MAXIMUM_PARAMETERS] = {};
};

// ── FLOATING_SAVE_AREA_32 ─────────────────────────────────────────

struct FloatingSaveArea32 {
    DWORD ControlWord   = 0;
    DWORD StatusWord    = 0;
    DWORD TagWord       = 0;
    DWORD ErrorOffset   = 0;
    DWORD ErrorSelector = 0;
    DWORD DataOffset    = 0;
    DWORD DataSelector  = 0;
    BYTE  RegisterArea[80] = {};
    DWORD Cr0NpxState   = 0;
};

// ── CONTEXT_32 ────────────────────────────────────────────────────

struct Context32 {
    DWORD ContextFlags  = 0;
    DWORD Dr0           = 0;
    DWORD Dr1           = 0;
    DWORD Dr2           = 0;
    DWORD Dr3           = 0;
    DWORD Dr6           = 0;
    DWORD Dr7           = 0;
    FloatingSaveArea32 FloatSave;
    DWORD SegGs         = 0;
    DWORD SegFs         = 0;
    DWORD SegEs         = 0;
    DWORD SegDs         = 0;
    DWORD Edi           = 0;
    DWORD Esi           = 0;
    DWORD Ebx           = 0;
    DWORD Edx           = 0;
    DWORD Ecx           = 0;
    DWORD Eax           = 0;
    DWORD Ebp           = 0;
    DWORD Eip           = 0;
    DWORD SegCs         = 0;
    DWORD EFlags        = 0;
    DWORD Esp           = 0;
    DWORD SegSs         = 0;
};

// ── LIST_ENTRY ────────────────────────────────────────────────────

struct ListEntry32 {
    DWORD Flink = 0;  // Pointer to next LIST_ENTRY_32
    DWORD Blink = 0;  // Pointer to prev LIST_ENTRY_32
};

struct ListEntry64 {
    uint64_t Flink = 0;
    uint64_t Blink = 0;
};

// ── UNICODE_STRING ────────────────────────────────────────────────

struct UnicodeString32 {
    WORD  Length        = 0;
    WORD  MaximumLength = 0;
    DWORD Buffer        = 0;  // Pointer to WCHAR string
};

struct UnicodeString64 {
    WORD     Length        = 0;
    WORD     MaximumLength = 0;
    uint64_t Buffer        = 0;
};

// ── EXCEPTION_REGISTRATION_RECORD (SEH chain) ────────────────────

struct ExceptionRegistrationRecord32 {
    DWORD pNext     = 0;  // Pointer to next SEH handler
    DWORD pfnHandler = 0;  // Handler address
};

struct ExceptionRegistrationRecord64 {
    uint64_t pNext     = 0;
    int64_t  pfnHandler = 0;
};

// ── CLIENT_ID ─────────────────────────────────────────────────────

struct ClientId32 {
    DWORD UniqueProcess = 0;
    DWORD UniqueThread  = 0;
};

struct ClientId64 {
    uint64_t UniqueProcess = 0;
    uint64_t UniqueThread  = 0;
};

// ── NT_TIB (Thread Information Block header) ─────────────────────

struct NtTib32 {
    DWORD ExceptionList       = 0;  // Ptr to ExceptionRegistrationRecord32
    DWORD StackBase           = 0;
    DWORD StackLimit          = 0;
    DWORD SubSystemTib        = 0;
    DWORD FiberData_Version   = 0;  // Union
    DWORD ArbitraryUserPointer = 0;
    DWORD Self                = 0;  // Pointer to self (TEB)
};

struct NtTib64 {
    uint64_t ExceptionList         = 0;
    uint64_t StackBase             = 0;
    uint64_t StackLimit            = 0;
    uint64_t SubSystemTib          = 0;
    uint64_t FiberData_Version     = 0;
    uint64_t ArbitraryUserPointer  = 0;
    uint64_t Self                  = 0;
};

// ── PEB_LDR_DATA (module list in PEB) ────────────────────────────

struct PebLdrData32 {
    ULONG      Length                            = 0;
    bool       Initialized                       = false;
    DWORD      SsHandle                          = 0;
    ListEntry32 InLoadOrderModuleList;
    ListEntry32 InMemoryOrderModuleList;
    ListEntry32 InInitializationOrderModuleList;
    DWORD      EntryInProgress                   = 0;
    bool       ShutdownInProgress                = false;
    DWORD      ShutdownThreadId                  = 0;
};

struct PebLdrData64 {
    ULONG      Length                            = 0;
    bool       Initialized                       = false;
    uint64_t   SsHandle                          = 0;
    ListEntry64 InLoadOrderModuleList;
    ListEntry64 InMemoryOrderModuleList;
    ListEntry64 InInitializationOrderModuleList;
    uint64_t   EntryInProgress                   = 0;
    bool       ShutdownInProgress                = false;
    uint64_t   ShutdownThreadId                  = 0;
};

// ── LDR_DATA_TABLE_ENTRY (module entry in PEB) ──────────────────

struct LdrDataTableEntry32 {
    ListEntry32     InLoadOrderLinks;
    ListEntry32     InMemoryOrderLinks;
    ListEntry32     InInitializationOrderLinks;
    DWORD           DllBase      = 0;
    DWORD           EntryPoint   = 0;
    ULONG           SizeOfImage  = 0;
    UnicodeString32 FullDllName;
    UnicodeString32 BaseDllName;
    ULONG           Flags        = 0;
    WORD            LoadCount    = 0;
    WORD            TlsIndex     = 0;
    ListEntry32     HashLinks;
    ULONG           TimeDateStamp = 0;
    DWORD           EntryPointActivationContext = 0;
    DWORD           PatchInformation = 0;
};

struct LdrDataTableEntry64 {
    ListEntry64     InLoadOrderLinks;
    ListEntry64     InMemoryOrderLinks;
    ListEntry64     InInitializationOrderLinks;
    uint64_t        DllBase      = 0;
    uint64_t        EntryPoint   = 0;
    ULONG           SizeOfImage  = 0;
    UnicodeString64 FullDllName;
    UnicodeString64 BaseDllName;
    ULONG           Flags        = 0;
    WORD            LoadCount    = 0;
    WORD            TlsIndex     = 0;
    ListEntry64     HashLinks;
    ULONG           TimeDateStamp = 0;
    uint64_t        EntryPointActivationContext = 0;
    uint64_t        PatchInformation = 0;
};

// ── PE32_IMAGE_CONFIG (Load Configuration) ──────────────────────

struct Pe32ImageConfig {
    DWORD Size                          = 0;
    DWORD TimeDateStamp                 = 0;
    WORD  MajorVersion                  = 0;
    WORD  MinorVersion                  = 0;
    DWORD GlobalFlagsClear              = 0;
    DWORD GlobalFlagsSet                = 0;
    DWORD CriticalSectionDefaultTimeout = 0;
    ULong64 DeCommitFreeBlockThreshold  = 0;
    ULong64 DeCommitTotalFreeThreshold  = 0;
    ULong64 LockPrefixTable             = 0;
    ULong64 MaximumAllocationSize       = 0;
    ULong64 VirtualMemoryThreshold      = 0;
    ULong64 ProcessAffinityMask         = 0;
    DWORD ProcessHeapFlags             = 0;
    WORD  CSDVersion                    = 0;
    WORD  DependentLoadFlags            = 0;
    ULong64 EditList                     = 0;
    ULong64 SecurityCookie               = 0;
    ULong64 SEHandlerTable               = 0;
    ULong64 SEHandlerCount               = 0;
};

// ── IMAGE_THUNK_DATA ──────────────────────────────────────────────

struct ImageThunkData32 {
    union {
        DWORD ForwarderString;   // PBYTE
        DWORD Function;          // PDWORD
        DWORD Ordinal;
        DWORD AddressOfData;     // PIMAGE_IMPORT_BY_NAME
    } u1;
};

struct ImageThunkData64 {
    union {
        uint64_t ForwarderString;
        uint64_t Function;
        uint64_t Ordinal;
        uint64_t AddressOfData;
    } u1;
};

#ifndef IMAGE_ORDINAL_FLAG32
constexpr DWORD IMAGE_ORDINAL_FLAG32 = 0x80000000;
#endif
#ifndef IMAGE_ORDINAL_FLAG64
constexpr uint64_t IMAGE_ORDINAL_FLAG64 = 0x8000000000000000ULL;
#endif

// ── IMAGE_IMPORT_BY_NAME ──────────────────────────────────────────

struct ImageImportByName {
    WORD Hint = 0;
    char Name[1];  // Variable length
};

// ── TPEB_32 (PEB for x86) ─────────────────────────────────────────
// Minimal structure with correct field offsets for initialization.
// Based on Pascal TPEB_32 (Core/tep_peb.pas) with {$PackRecords C}.

#pragma pack(push, 4)
struct Peb32 {
    uint8_t  InheritedAddressSpace;       // 0x00
    uint8_t  ReadImageFileExecOptions;    // 0x01
    uint8_t  BeingDebugged;               // 0x02
    uint8_t  SpareBool;                   // 0x03
    uint32_t Mutant;                      // 0x04
    uint32_t ImageBaseAddress;            // 0x08
    uint32_t Ldr;                         // 0x0C
    uint32_t ProcessParameters;           // 0x10
    uint32_t SubSystemData;               // 0x14
    uint32_t ProcessHeap;                 // 0x18
    uint32_t FastPebLock;                 // 0x1C
    uint32_t AtlThunkSListPtr;            // 0x20
    uint32_t IFEOKey;                     // 0x24
    uint32_t CrossProcessFlags;           // 0x28
    uint32_t KernelCallbackTable;         // 0x2C
    uint32_t SystemReserved;              // 0x30
    uint32_t AtlThunkSListPtr32;          // 0x34
    uint32_t ApiSetMap;                   // 0x38
    uint32_t TlsExpansionCounter;         // 0x3C
    uint32_t TlsBitmap;                   // 0x40
    uint32_t TlsBitmapBits[2];            // 0x44
    uint32_t ReadOnlySharedMemoryBase;    // 0x4C
    uint32_t HotpatchInformation;         // 0x50
    uint32_t ReadOnlyStaticServerData;    // 0x54
    uint32_t AnsiCodePageData;            // 0x58
    uint32_t OemCodePageData;             // 0x5C
    uint32_t UnicodeCaseTableData;        // 0x60
    uint32_t NumberOfProcessors;          // 0x64
    uint32_t NtGlobalFlag;                // 0x68
    uint32_t Unknown01;                   // 0x6C
    int64_t  CriticalSectionTimeout;      // 0x70
    uint32_t HeapSegmentReserve;          // 0x78
    uint32_t HeapSegmentCommit;           // 0x7C
    uint32_t HeapDeCommitTotalFreeThreshold; // 0x80
    uint32_t HeapDeCommitFreeBlockThreshold; // 0x84
    uint32_t NumberOfHeaps;               // 0x88
    uint32_t MaximumNumberOfHeaps;        // 0x8C
    uint32_t ProcessHeaps;                // 0x90
    uint32_t GdiSharedHandleTable;        // 0x94
    uint32_t ProcessStarterHelper;        // 0x98
    uint32_t GdiDCAttributeList;          // 0x9C
    uint32_t LoaderLock;                  // 0xA0
    uint32_t OSMajorVersion;              // 0xA4
    uint32_t OSMinorVersion;              // 0xA8
    uint16_t OSBuildNumber;               // 0xAC
    uint16_t OSCSDVersion;                // 0xAE
    uint32_t OSPlatformId;                // 0xB0
    uint32_t ImageSubsystem;              // 0xB4
    uint32_t ImageSubsystemMajorVersion;  // 0xB8
    uint32_t ImageSubsystemMinorVersion;  // 0xBC
    uint32_t ActiveProcessAffinityMask;   // 0xC0
    uint32_t GdiHandleBuffer[34];         // 0xC4
    uint32_t PostProcessInitRoutine;      // 0x14C
    uint32_t TlsExpansionBitmap;          // 0x150
    uint32_t TlsExpansionBitmapBits[32];  // 0x154
    uint32_t SessionId;                   // 0x1D4
    uint8_t  pad_1[0x288];                // remaining TPEB_32 fields → size = 0x45C
};
#pragma pack(pop)

static_assert(sizeof(Peb32) == 0x45C, "Peb32 size mismatch (expected 0x45C)");

// ── TPEB_64 (PEB for x64) ─────────────────────────────────────────
// Minimal structure matching Pascal TPEB_64 field ordering.

#pragma pack(push, 8)
struct Peb64 {
    uint8_t  InheritedAddressSpace;       // 0x00
    uint8_t  ReadImageFileExecOptions;    // 0x01
    uint8_t  BeingDebugged;               // 0x02
    uint8_t  SpareBool;                   // 0x03
    uint32_t Padding0;                    // 0x04
    uint64_t Mutant;                      // 0x08
    uint64_t ImageBaseAddress;            // 0x10
    int64_t  Ldr;                         // 0x18
    int64_t  ProcessParameters;           // 0x20
    uint64_t SubSystemData;               // 0x28
    uint64_t ProcessHeap;                 // 0x30
    uint64_t FastPebLock;                 // 0x38
    uint64_t AtlThunkSListPtr;            // 0x40
    uint64_t IFEOKey;                     // 0x48
    uint32_t CrossProcessFlags;           // 0x50
    uint32_t Padding1;                    // 0x54
    uint64_t KernelCallbackTable;         // 0x58
    uint32_t SystemReserved;              // 0x60
    uint32_t AtlThunkSListPtr32;          // 0x64
    uint64_t ApiSetMap;                   // 0x68
    uint32_t TlsExpansionCounter;         // 0x70
    uint32_t Padding2;                    // 0x74
    uint64_t TlsBitmap;                   // 0x78
    uint32_t TlsBitmapBits[2];            // 0x80
    uint64_t ReadOnlySharedMemoryBase;    // 0x88
    uint64_t HotpatchInformation;         // 0x90
    uint64_t ReadOnlyStaticServerData;    // 0x98
    uint64_t AnsiCodePageData;            // 0xA0
    uint64_t OemCodePageData;             // 0xA8
    uint64_t UnicodeCaseTableData;        // 0xB0
    uint32_t NumberOfProcessors;          // 0xB8
    uint32_t NtGlobalFlag;                // 0xBC
    int64_t  CriticalSectionTimeout;      // 0xC0
    uint64_t HeapSegmentReserve;          // 0xC8
    uint64_t HeapSegmentCommit;           // 0xD0
    uint64_t HeapDeCommitTotalFreeThreshold; // 0xD8
    uint64_t HeapDeCommitFreeBlockThreshold; // 0xE0
    uint32_t NumberOfHeaps;               // 0xE8
    uint32_t MaximumNumberOfHeaps;        // 0xEC
    uint64_t ProcessHeaps;                // 0xF0
    uint64_t GdiSharedHandleTable;        // 0xF8
    uint64_t ProcessStarterHelper;        // 0x100
    uint32_t GdiDCAttributeList;          // 0x108
    uint32_t Padding3;                    // 0x10C
    uint64_t LoaderLock;                  // 0x110
    uint32_t OSMajorVersion;              // 0x118
    uint32_t OSMinorVersion;              // 0x11C
    uint16_t OSBuildNumber;               // 0x120
    uint16_t OSCSDVersion;                // 0x122
    uint32_t OSPlatformId;                // 0x124
    uint32_t ImageSubsystem;              // 0x128
    uint32_t ImageSubsystemMajorVersion;  // 0x12C
    uint32_t ImageSubsystemMinorVersion;  // 0x130
    uint32_t Padding4;                    // 0x134
    uint64_t ActiveProcessAffinityMask;   // 0x138
    uint32_t GdiHandleBuffer[60];         // 0x140
    uint64_t PostProcessInitRoutine;      // 0x230
    uint64_t TlsExpansionBitmap;          // 0x238
    uint32_t TlsExpansionBitmapBits[32];  // 0x240
    uint32_t SessionId;                   // 0x2C0
    uint32_t Padding5;                    // 0x2C4
    uint8_t  pad_1[0x4D4];                // remaining TPEB_64 fields → size = 0x798
};
#pragma pack(pop)

static_assert(sizeof(Peb64) == 0x798, "Peb64 size mismatch (expected 0x798)");

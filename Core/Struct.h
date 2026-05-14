/**
 * Cmulator - PE / Windows structure definitions (C++ port of Core/struct.pas)
 *
 * Contains only the structures actively used by Cmulator's emulation core.
 * Full PE parsing is handled in src/pe/ (from Core/PE/*.pas).
 */

#pragma once

#include "types.h"

// ── IMAGE_DOS_HEADER ──────────────────────────────────────────────

struct ImageDosHeader {
    WORD  e_magic    = 0;    // Magic number (0x5A4D = "MZ")
    WORD  e_cblp     = 0;
    WORD  e_cp       = 0;
    WORD  e_crlc     = 0;
    WORD  e_cparhdr  = 0;
    WORD  e_minalloc = 0;
    WORD  e_maxalloc = 0;
    WORD  e_ss       = 0;
    WORD  e_sp       = 0;
    WORD  e_csum     = 0;
    WORD  e_ip       = 0;
    WORD  e_cs       = 0;
    WORD  e_lfarlc   = 0;
    WORD  e_ovno     = 0;
    WORD  e_res[4]   = {};
    WORD  e_oemid    = 0;
    WORD  e_oeminfo  = 0;
    WORD  e_res2[10] = {};
    LONG  e_lfanew   = 0;    // File address of NT header
};

static_assert(sizeof(ImageDosHeader) == 64, "ImageDosHeader size mismatch");

// ── IMAGE_FILE_HEADER ─────────────────────────────────────────────

struct ImageFileHeader {
    WORD  Machine              = 0;
    WORD  NumberOfSections     = 0;
    DWORD TimeDateStamp        = 0;
    DWORD PointerToSymbolTable = 0;
    DWORD NumberOfSymbols      = 0;
    WORD  SizeOfOptionalHeader = 0;
    WORD  Characteristics      = 0;
};

// ── IMAGE_DATA_DIRECTORY ──────────────────────────────────────────

struct ImageDataDirectory {
    DWORD VirtualAddress = 0;
    DWORD Size           = 0;
};

// ── IMAGE_OPTIONAL_HEADER32 ───────────────────────────────────────

#ifndef IMAGE_NUMBEROF_DIRECTORY_ENTRIES
constexpr int IMAGE_NUMBEROF_DIRECTORY_ENTRIES = 16;
#endif

struct ImageOptionalHeader32 {
    WORD  Magic                       = 0;
    BYTE  MajorLinkerVersion          = 0;
    BYTE  MinorLinkerVersion          = 0;
    DWORD SizeOfCode                  = 0;
    DWORD SizeOfInitializedData       = 0;
    DWORD SizeOfUninitializedData     = 0;
    DWORD AddressOfEntryPoint         = 0;
    DWORD BaseOfCode                  = 0;
    DWORD BaseOfData                  = 0;
    DWORD ImageBase                   = 0;
    DWORD SectionAlignment            = 0;
    DWORD FileAlignment               = 0;
    WORD  MajorOperatingSystemVersion = 0;
    WORD  MinorOperatingSystemVersion = 0;
    WORD  MajorImageVersion           = 0;
    WORD  MinorImageVersion           = 0;
    WORD  MajorSubsystemVersion       = 0;
    WORD  MinorSubsystemVersion       = 0;
    DWORD Win32VersionValue           = 0;
    DWORD SizeOfImage                 = 0;
    DWORD SizeOfHeaders               = 0;
    DWORD CheckSum                    = 0;
    WORD  Subsystem                   = 0;
    WORD  DllCharacteristics          = 0;
    DWORD SizeOfStackReserve          = 0;
    DWORD SizeOfStackCommit           = 0;
    DWORD SizeOfHeapReserve           = 0;
    DWORD SizeOfHeapCommit            = 0;
    DWORD LoaderFlags                 = 0;
    DWORD NumberOfRvaAndSizes         = 0;
    ImageDataDirectory DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES] = {};
};

// ── IMAGE_OPTIONAL_HEADER64 ───────────────────────────────────────

struct ImageOptionalHeader64 {
    WORD   Magic                       = 0;
    BYTE   MajorLinkerVersion          = 0;
    BYTE   MinorLinkerVersion          = 0;
    DWORD  SizeOfCode                  = 0;
    DWORD  SizeOfInitializedData       = 0;
    DWORD  SizeOfUninitializedData     = 0;
    DWORD  AddressOfEntryPoint         = 0;
    DWORD  BaseOfCode                  = 0;
    ULong64 ImageBase                  = 0;
    DWORD  SectionAlignment            = 0;
    DWORD  FileAlignment               = 0;
    WORD   MajorOperatingSystemVersion = 0;
    WORD   MinorOperatingSystemVersion = 0;
    WORD   MajorImageVersion           = 0;
    WORD   MinorImageVersion           = 0;
    WORD   MajorSubsystemVersion       = 0;
    WORD   MinorSubsystemVersion       = 0;
    DWORD  Win32VersionValue           = 0;
    DWORD  SizeOfImage                 = 0;
    DWORD  SizeOfHeaders               = 0;
    DWORD  CheckSum                    = 0;
    WORD   Subsystem                   = 0;
    WORD   DllCharacteristics          = 0;
    ULong64 SizeOfStackReserve         = 0;
    ULong64 SizeOfStackCommit          = 0;
    ULong64 SizeOfHeapReserve          = 0;
    ULong64 SizeOfHeapCommit           = 0;
    DWORD  LoaderFlags                 = 0;
    DWORD  NumberOfRvaAndSizes         = 0;
    ImageDataDirectory DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES] = {};
};

// ── IMAGE_NT_HEADERS ──────────────────────────────────────────────

struct ImageNtHeaders32 {
    DWORD                 Signature      = 0;
    ImageFileHeader       FileHeader;
    ImageOptionalHeader32 OptionalHeader;
};

struct ImageNtHeaders64 {
    DWORD                 Signature      = 0;
    ImageFileHeader       FileHeader;
    ImageOptionalHeader64 OptionalHeader;
};

// ── IMAGE_SECTION_HEADER ──────────────────────────────────────────

#ifndef IMAGE_SIZEOF_SHORT_NAME
constexpr int IMAGE_SIZEOF_SHORT_NAME = 8;
#endif

struct ImageSectionHeader {
    BYTE   Name[IMAGE_SIZEOF_SHORT_NAME] = {};
    DWORD  VirtualSize        = 0;
    DWORD  VirtualAddress     = 0;
    DWORD  SizeOfRawData      = 0;
    DWORD  PointerToRawData   = 0;
    DWORD  PointerToRelocations = 0;
    DWORD  PointerToLinenumbers = 0;
    WORD   NumberOfRelocations  = 0;
    WORD   NumberOfLinenumbers  = 0;
    DWORD  Characteristics      = 0;
};

// ── Common PE constants ───────────────────────────────────────────

#ifndef IMAGE_DOS_SIGNATURE
constexpr WORD  IMAGE_DOS_SIGNATURE     = 0x5A4D;  // "MZ"
#endif
#ifndef IMAGE_NT_SIGNATURE
constexpr DWORD IMAGE_NT_SIGNATURE      = 0x00004550; // "PE\0\0"
#endif
#ifndef IMAGE_NT_OPTIONAL_HDR32_MAGIC
constexpr WORD  IMAGE_NT_OPTIONAL_HDR32_MAGIC = 0x10b;
#endif
#ifndef IMAGE_NT_OPTIONAL_HDR64_MAGIC
constexpr WORD  IMAGE_NT_OPTIONAL_HDR64_MAGIC = 0x20b;
#endif

#ifndef IMAGE_FILE_MACHINE_I386
constexpr WORD IMAGE_FILE_MACHINE_I386  = 0x014c;
#endif
#ifndef IMAGE_FILE_MACHINE_AMD64
constexpr WORD IMAGE_FILE_MACHINE_AMD64 = 0x8664;
#endif

#ifndef IMAGE_FILE_EXECUTABLE_IMAGE
constexpr WORD IMAGE_FILE_EXECUTABLE_IMAGE = 0x0002;
#endif
#ifndef IMAGE_FILE_DLL
constexpr WORD IMAGE_FILE_DLL              = 0x2000;
#endif

// ── IMAGE_EXPORT_DIRECTORY ────────────────────────────────────────

struct ImageExportDirectory {
    DWORD Characteristics       = 0;
    DWORD TimeDateStamp         = 0;
    WORD  MajorVersion          = 0;
    WORD  MinorVersion          = 0;
    DWORD Name                  = 0;  // RVA to DLL name
    DWORD Base                  = 0;
    DWORD NumberOfFunctions     = 0;
    DWORD NumberOfNames         = 0;
    DWORD AddressOfFunctions    = 0;  // RVA
    DWORD AddressOfNames        = 0;  // RVA
    DWORD AddressOfNameOrdinals = 0;  // RVA
};

// ── IMAGE_IMPORT_DESCRIPTOR ───────────────────────────────────────

struct ImageImportDescriptor {
    DWORD OriginalFirstThunk = 0;  // RVA to IMAGE_THUNK_DATA
    DWORD TimeDateStamp      = 0;
    DWORD ForwarderChain     = 0;
    DWORD Name               = 0;  // RVA to DLL name
    DWORD FirstThunk         = 0;  // RVA to IMAGE_THUNK_DATA (IAT)
};

// ── IMAGE_TLS_DIRECTORY32 ─────────────────────────────────────────

struct ImageTlsDirectory32 {
    DWORD StartAddressOfRawData = 0;
    DWORD EndAddressOfRawData   = 0;
    DWORD AddressOfIndex        = 0;
    DWORD AddressOfCallBacks    = 0;
    DWORD SizeOfZeroFill        = 0;
    DWORD Characteristics       = 0;
};

struct ImageTlsDirectory64 {
    uint64_t StartAddressOfRawData = 0;
    uint64_t EndAddressOfRawData   = 0;
    uint64_t AddressOfIndex        = 0;
    uint64_t AddressOfCallBacks    = 0;
    DWORD    SizeOfZeroFill        = 0;
    DWORD    Characteristics       = 0;
};

// ── IMAGE_BASE_RELOCATION ─────────────────────────────────────────

struct ImageBaseRelocation {
    DWORD VirtualAddress = 0;
    DWORD SizeOfBlock    = 0;
};

#ifndef IMAGE_REL_BASED_ABSOLUTE
constexpr WORD IMAGE_REL_BASED_ABSOLUTE = 0;
#endif
#ifndef IMAGE_REL_BASED_HIGHLOW
constexpr WORD IMAGE_REL_BASED_HIGHLOW  = 3;
#endif
#ifndef IMAGE_REL_BASED_DIR64
constexpr WORD IMAGE_REL_BASED_DIR64    = 10;
#endif

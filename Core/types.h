/**
 * Cmulator - Common type definitions (C++ port)
 */

#pragma once

// On Windows, include Windows SDK before defining any types,
// to avoid conflicts with DWORD, ULONG, BYTE, WORD etc.
#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
#endif

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <stack>
#include <memory>

// ── Non-Windows types (guarded) ───────────────────────────────────

using WChar   = wchar_t;
using UInt    = uint32_t;
using UShort  = uint16_t;
using Short   = int16_t;
using WInt    = int32_t;
using Long    = int32_t;
using Long64  = int64_t;
using ULong64 = uint64_t;
using ULong32 = uint32_t;
using LongLong   = int64_t;
using ULongLong  = uint64_t;

// Windows SDK already defines DWORD, WORD, BYTE, ULONG
#ifndef _WIN32
  using UChar   = uint8_t;
  using DWORD   = uint32_t;
  using WORD    = uint16_t;
  using BYTE    = uint8_t;
  using ULONG   = uint32_t;
#else
  // On Windows, use SDK types. Add aliases for UChar if needed.
  using UChar   = uint8_t;
#endif

// Pascal string types
using AnsiString    = std::string;
using UnicodeString = std::wstring;

// Pascal-style naming aliases
using PQWord = uint64_t*;
using PByte  = uint8_t*;

// TMemoryStream
struct MemoryStream {
    std::vector<uint8_t> data;
    size_t position = 0;
};

// TRVA = Relative Virtual Address
using RVA = uint64_t;

// ByteArray
using ByteArray = std::vector<uint8_t>;

// ── STL aliases ───────────────────────────────────────────────────

template<typename K, typename V>
using FastHashMap = std::unordered_map<K, V>;

template<typename T>
using Stack = std::stack<T>;

// ── Constants ─────────────────────────────────────────────────────

constexpr uint64_t MICROSECONDS  = 1000000;
constexpr uint64_t UC_PAGE_SIZE  = 0x1000;
constexpr uint64_t EM_IMAGE_BASE = 0x400000;

constexpr const char* CM_VERSION = "v0.4.0";

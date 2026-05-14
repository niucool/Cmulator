/**
 * Cmulator - Global state (C++ port of Core/globals.pas)
 *
 * All global variables and config constants.
 * Pascal `var` globals → C++ `inline` variables (C++17)
 */

#pragma once

#include "types.h"

#include <string>

// Forward declarations (defined in emu.h)
class TEmu;

// ── QuickJS forward declares ──────────────────────────────────────
struct JSRuntime;
struct JSContext;

// ── Version ───────────────────────────────────────────────────────

// CM_VERSION already in types.h

// ── Verbosity flags ───────────────────────────────────────────────

inline bool VerboseExcp  = false;
inline bool Verbose      = false;
inline bool VerboseEx    = false;
inline bool VerboseExx   = false;
inline bool Speed        = false;
inline bool ShowASM      = false;
inline bool InterActive  = false; // TODO

// ── Step counters ─────────────────────────────────────────────────

inline uint64_t Steps_limit = 4000000; // 0 = unlimited
inline uint64_t Steps       = 0;

// ── Emulator singleton ────────────────────────────────────────────

/** Pascal: Emulator : TEmu; */
inline TEmu* Emulator = nullptr;

// ── QuickJS runtime globals ───────────────────────────────────────

/** Pascal: rt : JSRuntime = nil; */
inline JSRuntime* js_runtime = nullptr;

/** Pascal: ctx : JSContext = nil; */
inline JSContext* js_context = nullptr;

// ── JS Emu object (Pascal: JSEmu : JSValue) ──────────────────────
// JSValue is not a pointer — handled inline in code.

// ── DLL paths (from config.json) ──────────────────────────────────

/** Pascal: win32 : UnicodeString = ''; */
inline UnicodeString win32_path;

/** Pascal: win64 : UnicodeString = ''; */
inline UnicodeString win64_path;

/** Pascal: JSAPI : AnsiString = ''; */
inline AnsiString JSAPI_path;

/** Pascal: ApiSetSchemaPath : UnicodeString = ''; */
inline UnicodeString ApiSetSchemaPath;

// ── SEH globals ───────────────────────────────────────────────────

/** Pascal: Ident : Cardinal = 0; */
inline uint32_t Ident = 0;

/** Pascal: lastExceptionHandler : UInt64 = 0; */
inline uint64_t lastExceptionHandler = 0;

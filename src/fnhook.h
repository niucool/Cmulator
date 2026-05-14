/**
 * Cmulator - Hook function records (C++ port of Core/fnhook.pas)
 *
 * TLibFunction  — API function metadata (hooks counter, forwarder info, etc.)
 * THookFunction — Installed hook (JS callback + native callback)
 * TNewDll       — Loaded DLL record (base address, exports maps)
 */

#pragma once

#include "types.h"

#include <cstring>
#include <string>

// Forward declaration for QuickJS (defined in <quickjs.h>)
#include <quickjs.h>

// ── TLibFunction ──────────────────────────────────────────────────

struct TLibFunction {
    int32_t  Hits        = 0;     // how many times this API was called
    bool     IsForwarder = false;
    bool     IsOrdinal   = false;
    AnsiString LibName;
    AnsiString FuncName;
    AnsiString FWName;            // forwarder name
    uint64_t VAddress    = 0;     // virtual address in emulated memory
    uint32_t ordinal     = 0;
    PQWord   UserData    = nullptr; // pointer to user data
    uint64_t Return      = 0;     // return address (for OnExit callback)

    TLibFunction() = default;

    /** Pascal: TLibFunction.Create(LibName, FnName, VAddr, Ordinal, UserData, IsForwarder, IsOrdinal, FWName) */
    TLibFunction(const AnsiString& libName, const AnsiString& fnName,
                 uint64_t vAddr, uint32_t ord,
                 PQWord userData, bool isForwarder, bool isOrdinal,
                 const AnsiString& fwName)
        : Hits(0), IsForwarder(isForwarder), IsOrdinal(isOrdinal),
          LibName(libName), FuncName(fnName), FWName(fwName),
          VAddress(vAddr), ordinal(ord), UserData(userData), Return(0) {}
};

// ── THookFunction ─────────────────────────────────────────────────

struct THookFunction {
    AnsiString LibName;
    AnsiString FuncName;
    uint32_t  ordinal    = 0;
    bool      IsOrdinal  = false;
    TLibFunction API;

    using NativeCallback = bool(*)(uint64_t uc_engine_handle, uint64_t address, uint64_t ret);

    NativeCallback NativeCallBack = nullptr; // native callback (not yet used)

    struct JSAPI {
        JSValue JSHook;
        JSValue OnCallBack;
        JSValue OnExit;

        JSAPI() : JSHook(JS_UNDEFINED), OnCallBack(JS_UNDEFINED), OnExit(JS_UNDEFINED) {}
    } JSAPI;

    THookFunction() = default;

    /** Pascal: THookFunction.Create(libName, fnName, ordinal, isOrdinal, nCallBack, jsHook, onCallBack, onExit) */
    THookFunction(const AnsiString& libName, const AnsiString& fnName,
                  uint32_t ord, bool isOrdinal,
                  NativeCallback nCallBack,
                  JSValue jsHook, JSValue onCallBack, JSValue onExit)
        : LibName(libName), FuncName(fnName), ordinal(ord), IsOrdinal(isOrdinal),
          NativeCallBack(nCallBack)
    {
        JSAPI.JSHook     = jsHook;
        JSAPI.OnCallBack = onCallBack;
        JSAPI.OnExit     = onExit;
    }
};

// ── TNewDll ───────────────────────────────────────────────────────

struct TNewDll {
    AnsiString Dllname;
    AnsiString Path;              // full file path to the system DLL
    AnsiString version;

    uint64_t EntryPoint  = 0;
    uint64_t BaseAddress = 0;
    uint32_t ImageSize   = 0;

    uint64_t HookBase = 0;       // start of hook region
    uint64_t HookEnd  = 0;       // end of hook region

    // Export maps (key = hash/ordinal/address, value = function info)
    FastHashMap<uint64_t, TLibFunction> FnByAddr;
    FastHashMap<uint64_t, TLibFunction> FnByOrdinal;
    FastHashMap<uint64_t, TLibFunction> FnByName;

    TNewDll() = default;

    /** Pascal: TNewDll.Create(entryPoint, libName, baseAddress, imageSize, byAddr, byOrdinal, byName) */
    TNewDll(uint64_t entryPoint, const AnsiString& libName,
            uint64_t baseAddress, uint32_t imageSize,
            FastHashMap<uint64_t, TLibFunction> byAddr,
            FastHashMap<uint64_t, TLibFunction> byOrdinal,
            FastHashMap<uint64_t, TLibFunction> byName)
        : Dllname(libName), EntryPoint(entryPoint),
          BaseAddress(baseAddress), ImageSize(imageSize),
          FnByAddr(std::move(byAddr)),
          FnByOrdinal(std::move(byOrdinal)),
          FnByName(std::move(byName)) {}
};

// ── flush_r (Pascal: flush_r record) ──────────────────────────────

struct flush_r {
    uint64_t address = 0;
    int64_t  value   = 0;
    uint32_t size    = 0;
};

// ── TApiRed (Pascal: TApiRed record — ApiSet resolver) ────────────

struct TApiRed {
    uint8_t    count  = 0;
    AnsiString first;
    AnsiString last;
    AnsiString _alias;
};

// ── TApiInfo ──────────────────────────────────────────────────────

struct TApiInfo {
    bool ISAPI           = false;
    bool APIHandled      = false;
    bool CalledFromMainExe = false;
};

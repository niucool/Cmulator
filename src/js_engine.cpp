/**
 * Cmulator - JS Plugin Engine (C++ port of Core/jsplugins_engine.pas)
 * QuickJS initialization, ApiHook JS class, Emu JS object binding.
 */

#include "js_engine.h"
#include "globals.h"
#include "emu.h"
#include "fnhook.h"
#include "utils.h"
#include "pe_loader.h"

#include <plog/Log.h>
#include <xxhash.h>
#include <quickjs-libc.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

// ── JS Class ID ──────────────────────────────────────────────────

static JSClassID API_Class_id = 0;
static JSValue API_Class_Proto;
static JSClassDef JClass = { "ApiHook", nullptr, nullptr, nullptr, nullptr };

// ── logme: Pascal JS log function ────────────────────────────────

static JSValue logme(JSContext* ctx, JSValueConst this_val,
                     int argc, JSValueConst* argv, int magic) {
    (void)this_val;
    for (int i = 0; i < argc; i++) {
        if (i != 0) putchar(' ');
        const char* str = JS_ToCString(ctx, argv[i]);
        if (!str) return JS_EXCEPTION;
        fputs(str, stdout);
        JS_FreeCString(ctx, str);
    }
    putchar('\n');
    return JS_UNDEFINED;
}

// ── eval_buf / eval_file ─────────────────────────────────────────

static int eval_buf(JSContext* ctx, const char* buf, int buf_len,
                    const char* filename, int flags) {
    JSValue val = JS_Eval(ctx, buf, buf_len, filename, flags);
    if (JS_IsException(val)) {
        js_std_dump_error(ctx);
        return -1;
    }
    JS_FreeValue(ctx, val);
    return 0;
}

static int eval_file(JSContext* ctx, const char* filename, int flags) {
    size_t buf_len;
    void* buf = js_load_file(ctx, &buf_len, filename);
    if (!buf) {
        PLOG_ERROR << "Could not load: " << filename;
        return -1;
    }
    int ret = eval_buf(ctx, (const char*)buf, (int)buf_len, filename, flags);
    js_free(ctx, buf);
    return ret;
}

// ── NativeImportScripts (JS -> C) ────────────────────────────────

static JSValue NativeImportScripts(JSContext* ctx, JSValueConst this_val,
                                   int argc, JSValueConst* argv) {
    (void)this_val;
    for (int i = 0; i < argc; i++) {
        const char* fn = JS_ToCString(ctx, argv[i]);
        if (!fn) return JS_EXCEPTION;
        PLOG_DEBUG << "Importing JS: " << fn;
        if (eval_file(ctx, fn, JS_EVAL_TYPE_GLOBAL) < 0) {
            JS_FreeCString(ctx, fn);
            return JS_EXCEPTION;
        }
        JS_FreeCString(ctx, fn);
    }
    return JS_UNDEFINED;
}

// ── install: ApiHook.install(lib, name) ──────────────────────────

static JSValue apihook_install(JSContext* ctx, JSValueConst this_val,
                               int argc, JSValueConst* argv) {
    if (argc < 1) return JS_EXCEPTION;

    bool isAddress = (argc == 1);
    bool isOrdinal = false;
    uint32_t ordinal = 0;
    int64_t addrVal = 0;
    const char* lib = nullptr;
    const char* name = nullptr;

    if (isAddress) {
        if (JS_ToInt64(ctx, &addrVal, argv[0]) != 0)
            return JS_EXCEPTION;
    } else if (argc == 2) {
        lib = JS_ToCString(ctx, argv[0]);
        if (!lib) return JS_EXCEPTION;
        if (JS_IsString(argv[1])) {
            name = JS_ToCString(ctx, argv[1]);
        } else if (JS_ToUint32(ctx, &ordinal, argv[1]) == 0) {
            isOrdinal = true;
        } else {
            if (lib) JS_FreeCString(ctx, lib);
            return JS_EXCEPTION;
        }
    }

    JSValue onCB = JS_GetPropertyStr(ctx, this_val, "OnCallBack");
    JSValue onEx = JS_GetPropertyStr(ctx, this_val, "OnExit");

    if (JS_IsUndefined(onCB)) return JS_EXCEPTION;

    if (isAddress) {
        Emulator->Hooks.ByAddr[static_cast<uint64_t>(addrVal)] = THookFunction(
            "", "", 0, false, nullptr, this_val, onCB, onEx);
    } else if (isOrdinal) {
        std::string k = ExtractFileNameWithoutExt(lib) + "." + std::to_string(ordinal);
        uint64_t hash = XXH64(k.c_str(), k.size(), 0);
        Emulator->Hooks.ByOrdinal[hash] = THookFunction(
            lib, name ? name : "", ordinal, true, nullptr, this_val, onCB, onEx);
    } else {
        std::string k = ExtractFileNameWithoutExt(lib) + "." + (name ? name : "");
        uint64_t hash = XXH64(k.c_str(), k.size(), 0);
        Emulator->Hooks.ByName[hash] = THookFunction(
            lib, name ? name : "", 0, false, nullptr, this_val, onCB, onEx);
    }

    if (lib) JS_FreeCString(ctx, lib);
    if (name) JS_FreeCString(ctx, name);
    return JS_NewBool(ctx, true);
}

// ── ApiHook constructor ──────────────────────────────────────────

static JSValue ApiHook_ctor(JSContext* ctx, JSValueConst new_target,
                            int argc, JSValueConst* argv) {
    (void)argc; (void)argv;
    JSValue obj = JS_NewObjectProtoClass(ctx, API_Class_Proto, API_Class_id);
    JS_DefinePropertyValueStr(ctx, obj, "args", JS_NewArray(ctx),
                              JS_PROP_CONFIGURABLE | JS_PROP_WRITABLE);
    return obj;
}

// ── RegisterNativeClass ──────────────────────────────────────────

static void RegisterNativeClass(JSContext* ctx) {
    JS_NewClassID(JS_GetRuntime(ctx), &API_Class_id);
    JS_NewClass(JS_GetRuntime(ctx), API_Class_id, &JClass);

    API_Class_Proto = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, API_Class_Proto, "install",
        JS_NewCFunction(ctx, apihook_install, "install", 2));
    JS_SetClassProto(ctx, API_Class_id, API_Class_Proto);

    JSValue ctor = JS_NewCFunction2(ctx, ApiHook_ctor, "ApiHook", 1,
                                    JS_CFUNC_constructor, 0);
    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "ApiHook", ctor);
    JS_FreeValue(ctx, global);
}

// ── JS module loader stub ────────────────────────────────────────

static JSModuleDef* js_module_loader(JSContext* ctx,
                                     const char* module_name, void* opaque) {
    (void)ctx; (void)module_name; (void)opaque;
    return nullptr;
}

// ── Init_QJS ─────────────────────────────────────────────────────

void Init_QJS() {
    js_runtime = JS_NewRuntime();
    if (!js_runtime) { PLOG_FATAL << "JS_NewRuntime failed"; return; }

    js_context = JS_NewContext(js_runtime);
    if (!js_context) { PLOG_FATAL << "JS_NewContext failed"; return; }

    JS_SetModuleLoaderFunc(js_runtime, nullptr, js_module_loader, nullptr);

    // Register ApiHook class
    RegisterNativeClass(js_context);

    PLOG_INFO << "QuickJS initialized";
}

// ── Uninit_JSEngine ──────────────────────────────────────────────

void Uninit_JSEngine() {
    if (js_context) { JS_FreeContext(js_context); js_context = nullptr; }
    if (js_runtime) { JS_FreeRuntime(js_runtime); js_runtime = nullptr; }
}

// ── LoadScript ───────────────────────────────────────────────────

void LoadScript(const char* filename) {
    if (!js_context) return;
    if (eval_file(js_context, filename, JS_EVAL_TYPE_GLOBAL) < 0) {
        PLOG_ERROR << "Failed to load script: " << filename;
    }
}

// ── InitJSEmu: Register Emu object with all functions ────────────

void InitJSEmu() {
    if (!js_context || !Emulator) return;

    JSContext* ctx = js_context;
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue emuObj = JS_NewObject(ctx);

    // Properties
    JS_SetPropertyStr(ctx, emuObj, "TEB", JS_NewInt64(ctx, 0));  // TODO
    JS_SetPropertyStr(ctx, emuObj, "PEB", JS_NewInt64(ctx, 0));  // TODO
    JS_SetPropertyStr(ctx, emuObj, "isx64", JS_NewBool(ctx, Emulator->isx64));
    JS_SetPropertyStr(ctx, emuObj, "ImageBase", JS_NewInt64(ctx,
                       static_cast<int64_t>(Emulator->img.ImageBase)));
    JS_SetPropertyStr(ctx, emuObj, "Filename", JS_NewString(ctx,
                       Emulator->img.FileName.c_str()));

    // Functions registered via js_emuobj.h
    // Marked as handled in respective C functions
    JS_SetPropertyStr(ctx, global, "Emu", emuObj);

    // console.log override
    JSValue console = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, console, "log",
        JS_NewCFunctionMagic(ctx, logme, "log", 1, JS_CFUNC_generic_magic, 1));
    JS_SetPropertyStr(ctx, global, "console", console);

    JS_SetPropertyStr(ctx, global, "print",
        JS_NewCFunctionMagic(ctx, logme, "print", 1, JS_CFUNC_generic_magic, 0));
    JS_SetPropertyStr(ctx, global, "importScripts",
        JS_NewCFunction(ctx, NativeImportScripts, "importScripts", 1));

    JS_FreeValue(ctx, global);
}

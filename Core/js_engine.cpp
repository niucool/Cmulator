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

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// JS Class ID

static JSClassID API_Class_id = 0;
static JSValue API_Class_Proto;
static JSClassDef JClass = { "ApiHook", nullptr, nullptr, nullptr, nullptr };

// JS helper utilities.
static bool js_to_u64(JSContext* ctx, JSValueConst value, uint64_t& out,
                      const char* message) {
    int64_t tmp = 0;
    if (JS_ToInt64(ctx, &tmp, value) != 0) {
        JS_ThrowTypeError(ctx, "%s", message);
        return false;
    }
    out = static_cast<uint64_t>(tmp);
    return true;
}

static bool js_to_u32(JSContext* ctx, JSValueConst value, uint32_t& out,
                      const char* message) {
    if (JS_ToUint32(ctx, &out, value) != 0) {
        JS_ThrowTypeError(ctx, "%s", message);
        return false;
    }
    return true;
}

static bool js_to_std_string(JSContext* ctx, JSValueConst value,
                             std::string& out, const char* message) {
    const char* raw = JS_ToCString(ctx, value);
    if (!raw) {
        JS_ThrowTypeError(ctx, "%s", message);
        return false;
    }
    out.assign(raw);
    JS_FreeCString(ctx, raw);
    return true;
}

static std::string lower_module_name(const std::string& value) {
    std::string name = value;
    auto slash = name.find_last_of("/\\");
    if (slash != std::string::npos) name = name.substr(slash + 1);
    name = ExtractFileNameWithoutExt(name) + ".dll";
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
    return name;
}

static std::string file_name_only(const std::string& value) {
    auto slash = value.find_last_of("/\\");
    return slash == std::string::npos ? value : value.substr(slash + 1);
}

static bool require_emulator(JSContext* ctx) {
    if (Emulator && Emulator->uc) return true;
    JS_ThrowInternalError(ctx, "Emulator is not initialized");
    return false;
}

// logme: Pascal JS log function

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

// eval_buf / eval_file

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

// NativeImportScripts (JS -> C)

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

// install: ApiHook.install(lib, name)

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

// ApiHook constructor

static JSValue ApiHook_ctor(JSContext* ctx, JSValueConst new_target,
                            int argc, JSValueConst* argv) {
    (void)argc; (void)argv;
    JSValue obj = JS_NewObjectProtoClass(ctx, API_Class_Proto, API_Class_id);
    JS_DefinePropertyValueStr(ctx, obj, "args", JS_NewArray(ctx),
                              JS_PROP_CONFIGURABLE | JS_PROP_WRITABLE);
    return obj;
}

// RegisterNativeClass

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

// JS module loader stub

static JSModuleDef* js_module_loader(JSContext* ctx,
                                     const char* module_name, void* opaque) {
    (void)ctx; (void)module_name; (void)opaque;
    return nullptr;
}

// Init_QJS

void Init_QJS() {
    rt = JS_NewRuntime();
    if (!rt) { PLOG_FATAL << "JS_NewRuntime failed"; return; }

    ctx = JS_NewContext(rt);
    if (!ctx) { PLOG_FATAL << "JS_NewContext failed"; return; }

    JS_SetModuleLoaderFunc(rt, nullptr, js_module_loader, nullptr);

    // Register ApiHook class
    RegisterNativeClass(ctx);

    PLOG_INFO << "QuickJS initialized";
}

// Uninit_JSEngine

void Uninit_JSEngine() {
    if (ctx) { JS_FreeContext(ctx); ctx = nullptr; }
    if (rt) { JS_FreeRuntime(rt); rt = nullptr; }
}

// LoadScript

void LoadScript(const char* filename) {
    if (!ctx) return;
    if (eval_file(ctx, filename, JS_EVAL_TYPE_GLOBAL) < 0) {
        PLOG_ERROR << "Failed to load script: " << filename;
    }
}

// Emu JS object methods.
static JSValue emu_ReadReg(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (!require_emulator(ctx)) return JS_EXCEPTION;
    if (argc != 1) return JS_ThrowTypeError(ctx, "ReadReg(reg) expects one numeric argument");
    uint32_t reg = 0;
    if (!js_to_u32(ctx, argv[0], reg, "ReadReg reg must be a number")) return JS_EXCEPTION;
    uint64_t value = 0;
    Emulator->err = uc_reg_read(Emulator->uc, reg, &value);
    return JS_NewInt64(ctx, static_cast<int64_t>(value));
}

static JSValue emu_SetReg(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (!require_emulator(ctx)) return JS_EXCEPTION;
    if (argc != 2) return JS_ThrowTypeError(ctx, "SetReg(reg, value) expects two numeric arguments");
    uint32_t reg = 0;
    uint64_t value = 0;
    if (!js_to_u32(ctx, argv[0], reg, "SetReg reg must be a number")) return JS_EXCEPTION;
    if (!js_to_u64(ctx, argv[1], value, "SetReg value must be a number")) return JS_EXCEPTION;
    Emulator->err = uc_reg_write(Emulator->uc, reg, &value);
    return JS_NewBool(ctx, Emulator->err == UC_ERR_OK);
}

static JSValue emu_ReadStringA(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc < 1 || argc > 2) return JS_ThrowTypeError(ctx, "ReadStringA(addr[, len]) expects one or two arguments");
    uint64_t addr = 0;
    uint32_t len = 0;
    if (!js_to_u64(ctx, argv[0], addr, "ReadStringA addr must be a number")) return JS_EXCEPTION;
    if (argc == 2 && !js_to_u32(ctx, argv[1], len, "ReadStringA len must be a number")) return JS_EXCEPTION;
    return JS_NewString(ctx, ReadStringA(addr, len).c_str());
}

static JSValue emu_ReadStringW(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc < 1 || argc > 2) return JS_ThrowTypeError(ctx, "ReadStringW(addr[, len]) expects one or two arguments");
    uint64_t addr = 0;
    uint32_t len = 0;
    if (!js_to_u64(ctx, argv[0], addr, "ReadStringW addr must be a number")) return JS_EXCEPTION;
    if (argc == 2 && !js_to_u32(ctx, argv[1], len, "ReadStringW len must be a number")) return JS_EXCEPTION;
    return JS_NewString(ctx, ReadStringW(addr, len).c_str());
}

static JSValue emu_WriteStringA(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc != 2) return JS_ThrowTypeError(ctx, "WriteStringA(addr, string) expects two arguments");
    uint64_t addr = 0;
    std::string value;
    if (!js_to_u64(ctx, argv[0], addr, "WriteStringA addr must be a number")) return JS_EXCEPTION;
    if (!js_to_std_string(ctx, argv[1], value, "WriteStringA value must be a string")) return JS_EXCEPTION;
    return JS_NewInt32(ctx, static_cast<int32_t>(WriteStringA(addr, value)));
}

static JSValue emu_WriteStringW(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc != 2) return JS_ThrowTypeError(ctx, "WriteStringW(addr, string) expects two arguments");
    uint64_t addr = 0;
    std::string value;
    if (!js_to_u64(ctx, argv[0], addr, "WriteStringW addr must be a number")) return JS_EXCEPTION;
    if (!js_to_std_string(ctx, argv[1], value, "WriteStringW value must be a string")) return JS_EXCEPTION;
    return JS_NewInt32(ctx, static_cast<int32_t>(WriteStringW(addr, value)));
}

static JSValue emu_LoadLibrary(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (!require_emulator(ctx)) return JS_EXCEPTION;
    if (argc != 1) return JS_ThrowTypeError(ctx, "LoadLibrary(name) expects one string argument");
    std::string raw;
    if (!js_to_std_string(ctx, argv[0], raw, "LoadLibrary name must be a string")) return JS_EXCEPTION;
    std::string name = lower_module_name(raw);
    auto it = Emulator->Libs.find(name);
    if (it == Emulator->Libs.end()) {
        if (!load_sys_dll(Emulator->uc, name)) return JS_NewInt64(ctx, 0);
        it = Emulator->Libs.find(name);
    }
    return JS_NewInt64(ctx, it == Emulator->Libs.end() ? 0 : static_cast<int64_t>(it->second.BaseAddress));
}

static JSValue emu_GetModuleName(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (!Emulator) return JS_EXCEPTION;
    if (argc == 0) return JS_NewString(ctx, file_name_only(Emulator->img.FileName).c_str());
    if (argc != 1) return JS_ThrowTypeError(ctx, "GetModuleName([handle]) expects zero or one argument");
    uint64_t handle = 0;
    if (!js_to_u64(ctx, argv[0], handle, "GetModuleName handle must be a number")) return JS_EXCEPTION;
    if (handle == 0 || handle == Emulator->img.ImageBase)
        return JS_NewString(ctx, file_name_only(Emulator->img.FileName).c_str());
    for (const auto& item : Emulator->Libs) {
        if (item.second.BaseAddress == handle)
            return JS_NewString(ctx, file_name_only(item.second.Dllname).c_str());
    }
    return JS_NewString(ctx, "");
}

static JSValue emu_GetModuleHandle(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (!Emulator) return JS_EXCEPTION;
    if (argc == 0) return JS_NewInt64(ctx, static_cast<int64_t>(Emulator->img.ImageBase));
    if (argc != 1) return JS_ThrowTypeError(ctx, "GetModuleHandle([name]) expects zero or one argument");
    std::string name;
    if (!js_to_std_string(ctx, argv[0], name, "GetModuleHandle name must be a string")) return JS_EXCEPTION;
    return JS_NewInt64(ctx, static_cast<int64_t>(CmuGetModulehandle(name)));
}

static JSValue emu_GetProcAddress(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc != 2) return JS_ThrowTypeError(ctx, "GetProcAddress(handle, name) expects two arguments");
    uint64_t handle = 0;
    std::string name;
    if (!js_to_u64(ctx, argv[0], handle, "GetProcAddress handle must be a number")) return JS_EXCEPTION;
    if (!js_to_std_string(ctx, argv[1], name, "GetProcAddress name must be a string")) return JS_EXCEPTION;
    return JS_NewInt64(ctx, static_cast<int64_t>(GetProcAddr(handle, name)));
}

static JSValue emu_WriteByte(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc != 2) return JS_ThrowTypeError(ctx, "WriteByte(addr, value) expects two arguments");
    uint64_t addr = 0; uint32_t value = 0;
    if (!js_to_u64(ctx, argv[0], addr, "WriteByte addr must be a number")) return JS_EXCEPTION;
    if (!js_to_u32(ctx, argv[1], value, "WriteByte value must be a number")) return JS_EXCEPTION;
    return JS_NewBool(ctx, WriteByte(addr, static_cast<uint8_t>(value)));
}

static JSValue emu_WriteWord(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc != 2) return JS_ThrowTypeError(ctx, "WriteWord(addr, value) expects two arguments");
    uint64_t addr = 0; uint32_t value = 0;
    if (!js_to_u64(ctx, argv[0], addr, "WriteWord addr must be a number")) return JS_EXCEPTION;
    if (!js_to_u32(ctx, argv[1], value, "WriteWord value must be a number")) return JS_EXCEPTION;
    return JS_NewBool(ctx, WriteWord(addr, static_cast<uint16_t>(value)));
}

static JSValue emu_WriteDword(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc != 2) return JS_ThrowTypeError(ctx, "WriteDword(addr, value) expects two arguments");
    uint64_t addr = 0; uint32_t value = 0;
    if (!js_to_u64(ctx, argv[0], addr, "WriteDword addr must be a number")) return JS_EXCEPTION;
    if (!js_to_u32(ctx, argv[1], value, "WriteDword value must be a number")) return JS_EXCEPTION;
    return JS_NewBool(ctx, WriteDword(addr, value));
}

static JSValue emu_WriteQword(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc != 2) return JS_ThrowTypeError(ctx, "WriteQword(addr, value) expects two arguments");
    uint64_t addr = 0; uint64_t value = 0;
    if (!js_to_u64(ctx, argv[0], addr, "WriteQword addr must be a number")) return JS_EXCEPTION;
    if (!js_to_u64(ctx, argv[1], value, "WriteQword value must be a number")) return JS_EXCEPTION;
    return JS_NewBool(ctx, WriteQword(addr, value));
}

static JSValue emu_WriteMem(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc != 2) return JS_ThrowTypeError(ctx, "WriteMem(addr, array) expects two arguments");
    uint64_t addr = 0;
    if (!js_to_u64(ctx, argv[0], addr, "WriteMem addr must be a number")) return JS_EXCEPTION;
    if (!JS_IsArray(argv[1])) return JS_ThrowTypeError(ctx, "WriteMem data must be an array");
    JSValue lengthValue = JS_GetPropertyStr(ctx, argv[1], "length");
    uint32_t len = 0;
    bool ok = js_to_u32(ctx, lengthValue, len, "WriteMem array length must be numeric");
    JS_FreeValue(ctx, lengthValue);
    if (!ok) return JS_EXCEPTION;
    uint32_t written = 0;
    for (uint32_t i = 0; i < len; ++i) {
        JSValue element = JS_GetPropertyUint32(ctx, argv[1], i);
        uint32_t byteValue = 0;
        ok = js_to_u32(ctx, element, byteValue, "WriteMem array elements must be numbers");
        JS_FreeValue(ctx, element);
        if (!ok) return JS_EXCEPTION;
        if (!WriteByte(addr + i, static_cast<uint8_t>(byteValue))) break;
        written = i + 1;
    }
    return JS_NewUint32(ctx, written);
}

static JSValue emu_ReadByte(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc != 1) return JS_ThrowTypeError(ctx, "ReadByte(addr) expects one argument");
    uint64_t addr = 0;
    if (!js_to_u64(ctx, argv[0], addr, "ReadByte addr must be a number")) return JS_EXCEPTION;
    return JS_NewUint32(ctx, ReadByte(addr));
}

static JSValue emu_ReadWord(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc != 1) return JS_ThrowTypeError(ctx, "ReadWord(addr) expects one argument");
    uint64_t addr = 0;
    if (!js_to_u64(ctx, argv[0], addr, "ReadWord addr must be a number")) return JS_EXCEPTION;
    return JS_NewUint32(ctx, ReadWord(addr));
}

static JSValue emu_ReadDword(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc != 1) return JS_ThrowTypeError(ctx, "ReadDword(addr) expects one argument");
    uint64_t addr = 0;
    if (!js_to_u64(ctx, argv[0], addr, "ReadDword addr must be a number")) return JS_EXCEPTION;
    return JS_NewInt32(ctx, ReadDword(addr));
}

static JSValue emu_ReadQword(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc != 1) return JS_ThrowTypeError(ctx, "ReadQword(addr) expects one argument");
    uint64_t addr = 0;
    if (!js_to_u64(ctx, argv[0], addr, "ReadQword addr must be a number")) return JS_EXCEPTION;
    return JS_NewInt64(ctx, ReadQword(addr));
}

static JSValue emu_ReadMem(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc != 2) return JS_ThrowTypeError(ctx, "ReadMem(addr, len) expects two arguments");
    uint64_t addr = 0; uint32_t len = 0;
    if (!js_to_u64(ctx, argv[0], addr, "ReadMem addr must be a number")) return JS_EXCEPTION;
    if (!js_to_u32(ctx, argv[1], len, "ReadMem len must be a number")) return JS_EXCEPTION;
    ByteArray data = ReadMem(addr, len);
    JSValue array = JS_NewArray(ctx);
    for (uint32_t i = 0; i < data.size(); ++i)
        JS_SetPropertyUint32(ctx, array, i, JS_NewUint32(ctx, data[i]));
    return array;
}

static JSValue emu_push(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc != 1) return JS_ThrowTypeError(ctx, "push(value) expects one argument");
    uint64_t value = 0;
    if (!js_to_u64(ctx, argv[0], value, "push value must be a number")) return JS_EXCEPTION;
    return JS_NewBool(ctx, push_value(value));
}

static JSValue emu_pop(JSContext* ctx, JSValueConst, int argc, JSValueConst*) {
    if (argc != 0) return JS_ThrowTypeError(ctx, "pop() expects no arguments");
    return JS_NewInt64(ctx, static_cast<int64_t>(pop_value()));
}

static JSValue emu_Stop(JSContext*, JSValueConst, int, JSValueConst*) {
    if (Emulator) Emulator->Stop = true;
    return JS_TRUE;
}

static JSValue emu_LastError(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    return JS_NewString(ctx, Emulator ? uc_strerror(Emulator->err) : "no emulator");
}

static JSValue emu_HexDump(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc < 2 || argc > 3) return JS_ThrowTypeError(ctx, "HexDump(addr, len[, cols]) expects two or three arguments");
    uint64_t addr = 0; uint32_t len = 0; uint32_t cols = 16;
    if (!js_to_u64(ctx, argv[0], addr, "HexDump addr must be a number")) return JS_EXCEPTION;
    if (!js_to_u32(ctx, argv[1], len, "HexDump len must be a number")) return JS_EXCEPTION;
    if (argc == 3 && !js_to_u32(ctx, argv[2], cols, "HexDump cols must be a number")) return JS_EXCEPTION;
    ByteArray data = ReadMem(addr, len);
    if (!data.empty()) HexDump(data.data(), static_cast<int>(data.size()), addr, static_cast<int>(cols));
    return JS_UNDEFINED;
}

static JSValue emu_StackDump(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc != 2) return JS_ThrowTypeError(ctx, "StackDump(addr, len) expects two arguments");
    uint64_t addr = 0; uint32_t len = 0;
    if (!js_to_u64(ctx, argv[0], addr, "StackDump addr must be a number")) return JS_EXCEPTION;
    if (!js_to_u32(ctx, argv[1], len, "StackDump len must be a number")) return JS_EXCEPTION;
    DumpStack(addr, len);
    return JS_UNDEFINED;
}

static void register_emu_method(JSContext* ctx, JSValue obj, const char* name,
                                JSCFunction* fn, int length) {
    JS_SetPropertyStr(ctx, obj, name, JS_NewCFunction(ctx, fn, name, length));
}

// InitJSEmu: Register Emu object with all functions.

void InitJSEmu() {
    if (!ctx || !Emulator) return;

    JSContext* c = ctx;
    JSValue global = JS_GetGlobalObject(c);
    JSValue emuObj = JS_NewObject(c);

    // Properties
    JS_SetPropertyStr(c, emuObj, "TEB", JS_NewInt64(c,
                       static_cast<int64_t>(Emulator->TEB_Address)));
    JS_SetPropertyStr(ctx, emuObj, "PEB", JS_NewInt64(ctx,
                       static_cast<int64_t>(Emulator->PEB_address)));
    JS_SetPropertyStr(ctx, emuObj, "isx64", JS_NewBool(ctx, Emulator->isx64));
    JS_SetPropertyStr(ctx, emuObj, "ImageBase", JS_NewInt64(ctx,
                       static_cast<int64_t>(Emulator->img.ImageBase)));
    JS_SetPropertyStr(ctx, emuObj, "Filename", JS_NewString(ctx,
                       Emulator->img.FileName.c_str()));

    register_emu_method(c, emuObj, "ReadReg", emu_ReadReg, 1);
    register_emu_method(c, emuObj, "SetReg", emu_SetReg, 2);
    register_emu_method(c, emuObj, "ReadStringA", emu_ReadStringA, 2);
    register_emu_method(c, emuObj, "ReadStringW", emu_ReadStringW, 2);
    register_emu_method(c, emuObj, "WriteStringA", emu_WriteStringA, 2);
    register_emu_method(c, emuObj, "WriteStringW", emu_WriteStringW, 2);
    register_emu_method(c, emuObj, "LoadLibrary", emu_LoadLibrary, 1);
    register_emu_method(c, emuObj, "GetModuleName", emu_GetModuleName, 1);
    register_emu_method(c, emuObj, "GetModuleHandle", emu_GetModuleHandle, 1);
    register_emu_method(c, emuObj, "GetProcAddress", emu_GetProcAddress, 2);
    register_emu_method(c, emuObj, "GetProcAddr", emu_GetProcAddress, 2);
    register_emu_method(c, emuObj, "WriteByte", emu_WriteByte, 2);
    register_emu_method(c, emuObj, "WriteWord", emu_WriteWord, 2);
    register_emu_method(c, emuObj, "WriteDword", emu_WriteDword, 2);
    register_emu_method(c, emuObj, "WriteQword", emu_WriteQword, 2);
    register_emu_method(c, emuObj, "WriteMem", emu_WriteMem, 2);
    register_emu_method(c, emuObj, "ReadByte", emu_ReadByte, 1);
    register_emu_method(c, emuObj, "ReadWord", emu_ReadWord, 1);
    register_emu_method(c, emuObj, "ReadDword", emu_ReadDword, 1);
    register_emu_method(c, emuObj, "ReadQword", emu_ReadQword, 1);
    register_emu_method(c, emuObj, "ReadMem", emu_ReadMem, 2);
    register_emu_method(c, emuObj, "push", emu_push, 1);
    register_emu_method(c, emuObj, "pop", emu_pop, 0);
    register_emu_method(c, emuObj, "Stop", emu_Stop, 0);
    register_emu_method(c, emuObj, "LastError", emu_LastError, 0);
    register_emu_method(c, emuObj, "HexDump", emu_HexDump, 3);
    register_emu_method(c, emuObj, "StackDump", emu_StackDump, 2);

    JS_SetPropertyStr(c, global, "Emu", emuObj);
    // console.log override
    JSValue console = JS_NewObject(c);
    JS_SetPropertyStr(c, console, "log",
        JS_NewCFunctionMagic(c, logme, "log", 1, JS_CFUNC_generic_magic, 1));
    JS_SetPropertyStr(c, global, "console", console);

    JS_SetPropertyStr(c, global, "print",
        JS_NewCFunctionMagic(c, logme, "print", 1, JS_CFUNC_generic_magic, 0));
    JS_SetPropertyStr(c, global, "importScripts",
        JS_NewCFunction(c, NativeImportScripts, "importScripts", 1));

    JS_FreeValue(c, global);
}
